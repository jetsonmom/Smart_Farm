import cv2
import time
import schedule
from datetime import datetime
import os
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.image import MIMEImage
import numpy as np
import serial
import threading
import logging
from pathlib import Path

# 로깅 설정
log_dir = Path('logs')
log_dir.mkdir(exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(log_dir / 'plant_monitor.log'),
        logging.StreamHandler()
    ]
)

# 시리얼 통신 설정
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(2)  # ESP32 리셋 대기

def send_time_to_esp32():
    """ESP32에 현재 시간 전송"""
    while True:
        try:
            current_time = datetime.now().strftime("%H:%M:%S")
            command = f"TIME:{current_time}\n"
            ser.write(command.encode())
            
            # ESP32로부터 상태 받기
            response = ser.readline().decode().strip()
            if response.startswith("STATUS:"):
                status = response.split(":")[1].split(",")
                logging.info(f"장치 상태 - LED: {status[0]}, 펌프: {status[1]}, 팬: {status[2]}")
            
            time.sleep(1)
        except Exception as e:
            logging.error(f"ESP32 통신 오류: {str(e)}")
            time.sleep(5)

def gstreamer_pipeline(
    sensor_id=0,
    capture_width=1280,
    capture_height=720,
    display_width=1280,
    display_height=720,
    framerate=30,
    flip_method=0,
    exposure_time=13000,
    gain=(1.0, 2.5),
    saturation=1.5,
    contrast=1.2,
    brightness=0.0
):
    return (
        f"nvarguscamerasrc sensor-id={sensor_id} "
        f"exposuretimerange='{exposure_time} {exposure_time}' "
        f"gainrange='{gain[0]} {gain[1]}' "
        f"saturation={saturation} "
        f"contrast={contrast} "
        f"brightness={brightness} "
        f"aeLock=True wbmode=3 ! "
        f"video/x-raw(memory:NVMM), width=(int){capture_width}, height=(int){capture_height}, "
        f"format=(string)NV12, framerate=(fraction){framerate}/1 ! "
        f"nvvidconv flip-method={flip_method} ! "
        f"video/x-raw, width=(int){display_width}, height=(int){display_height}, format=(string)BGRx ! "
        "videoconvert ! "
        "video/x-raw, format=(string)BGR ! appsink"
    )

def enhance_image(image):
    try:
        kernel = np.array([[-1,-1,-1], [-1,9,-1], [-1,-1,-1]])
        sharpened = cv2.filter2D(image, -1, kernel)
        
        lab = cv2.cvtColor(sharpened, cv2.COLOR_BGR2LAB)
        l, a, b = cv2.split(lab)
        clahe = cv2.createCLAHE(clipLimit=3.0, tileGridSize=(8,8))
        cl = clahe.apply(l)
        limg = cv2.merge((cl,a,b))
        enhanced = cv2.cvtColor(limg, cv2.COLOR_LAB2BGR)
        
        return enhanced
    except Exception as e:
        logging.error(f"이미지 향상 중 오류 발생: {str(e)}")
        return image

def send_email(file_path):
    email_address = "jmerrier0910@gmail.com"
    app_password = "smvrcqoizxbxmyhy"

    try:
        message = MIMEMultipart()
        message["From"] = email_address
        message["To"] = email_address
        message["Subject"] = f"Plant Monitoring Image - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"

        body = "식물 모니터링 이미지가 첨부되어 있습니다."
        message.attach(MIMEText(body, "plain"))

        with open(file_path, "rb") as image_file:
            image = MIMEImage(image_file.read(), name=os.path.basename(file_path))
            message.attach(image)

        with smtplib.SMTP_SSL("smtp.gmail.com", 465) as server:
            server.login(email_address, app_password)
            server.send_message(message)
            
        logging.info("이메일이 성공적으로 전송되었습니다.")
        return True
    except Exception as e:
        logging.error(f"이메일 전송 중 오류: {str(e)}")
        return False

def capture_image():
    logging.info("사진 촬영 시작...")
    video_capture = None
    
    try:
        video_capture = cv2.VideoCapture(gstreamer_pipeline(flip_method=0), cv2.CAP_GSTREAMER)
        
        if not video_capture.isOpened():
            logging.error("카메라를 열 수 없습니다.")
            return False

        time.sleep(2)
        
        ret_val, frame = video_capture.read()
        if not ret_val:
            logging.error("프레임을 읽을 수 없습니다.")
            return False

        enhanced_frame = enhance_image(frame)
        
        current_time = datetime.now()
        folder_name = current_time.strftime("%Y%m%d")
        Path(folder_name).mkdir(exist_ok=True)
        
        file_name = current_time.strftime("%Y%m%d_%H%M%S.jpg")
        file_path = str(Path(folder_name) / file_name)
        
        cv2.imwrite(file_path, enhanced_frame)
        logging.info(f"사진이 저장되었습니다: {file_path}")
        
        if send_email(file_path):
            logging.info("작업이 성공적으로 완료되었습니다.")
            return True
        return False

    except Exception as e:
        logging.error(f"사진 촬영 중 오류 발생: {str(e)}")
        return False
    finally:
        if video_capture is not None:
            video_capture.release()

def run_capture_with_retry(max_retries=3):
    for attempt in range(max_retries):
        if capture_image():
            return True
        logging.warning(f"시도 {attempt + 1}/{max_retries} 실패, 재시도 중...")
        time.sleep(5)
    logging.error("최대 재시도 횟수를 초과했습니다.")
    return False

if __name__ == "__main__":
    logging.info("식물 모니터링 스크립트가 시작되었습니다.")
    
    # ESP32 통신 스레드 시작
    esp32_thread = threading.Thread(target=send_time_to_esp32, daemon=True)
    esp32_thread.start()
    
    # 스케줄 설정
    schedule.every().day.at("05:00").do(run_capture_with_retry)
    schedule.every().day.at("12:00").do(run_capture_with_retry)
    schedule.every().day.at("18:50").do(run_capture_with_retry)
    schedule.every().day.at("20:50").do(run_capture_with_retry)
    
    try:
        while True:
            schedule.run_pending()
            time.sleep(1)
    except KeyboardInterrupt:
        logging.info("프로그램이 사용자에 의해 중단되었습니다.")
        ser.close()
    except Exception as e:
        logging.error(f"예상치 못한 오류 발생: {str(e)}")
        ser.close()
