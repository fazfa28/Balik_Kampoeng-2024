
from PIL import Image
import os as os
import requests
import base64
import os
import sys
import re
from datetime import datetime
import itertools
from pytz import timezone, utc


class HandleMaster:
    def __init__(self, img_path, predicted_img_name, mode, weights = "/home/trapcam/path_to_python_script/best.pt"):
      self.img_path = img_path
      self.img_name = predicted_img_name
      self.weights = weights
      self.confidence = None
      gmt7 = timezone("Asia/Bangkok")
      self.datetime = datetime.now(utc).astimezone(gmt7).strftime("%Y-%m-%d %H:%M:%S")
      self.is_detected = None
      self.mode = mode
      
      self.morning_report = "/home/trapcam/morning_report"
      self.night_report = "/home/trapcam/night_report"

      if self.mode == "1":
          print("image name: ",self.img_name)
          self.slave = re.search(r'.*_esp32-cam-(\d+)\.jpg', self.img_name).group(1)
          try:
              from ultralytics import YOLO
              self.YOLO = YOLO
          except ImportError as e:
              print(f"Failed to import YOLO: {e}")
              self.YOLO = None

    def predict(self, img_path, weights):
      if self.YOLO is None:
            print("YOLO model is not available.")
            return None
      model = self.YOLO(weights) 
      result = model(img_path)[0]
      return result
    
    def analyze_results(self,result):
      """"
      Analyse the result to print whether the image does contain orangutan or not
      Returns:
        is_detected (bool): True if orangutan is detected
      """
      detection_string = result.verbose()
      im_bgr = result.plot()  # BGR-order numpy array
      im_rgb = Image.fromarray(im_bgr[..., ::-1])  # Convert BGR to RGB-order PIL image
      
      if "no detections" in detection_string:
          print(result.path, "doesn't contain orangutan")
      else:
          print(result.path, "contains orang utan")
      
      try:
          float_tensor = float(result.boxes.conf[0].item())
          self.confidence = round(float_tensor, 2)
          print(self.confidence)
          if self.confidence > 0.6:
              self.is_detected = True
          else:
              self.is_detected = False
          # Save results to disk
          result.save(filename=f"/home/trapcam/path_to_python_script/predicted_images/{self.img_name}")
      except:
          self.is_detected = False
          result.save(filename=f"/home/trapcam/path_to_python_script/predicted_images/{self.img_name}")
          self.confidence = 1

    def make_caption(self):
      print(self.is_detected)
      if self.is_detected:
        return f"<b>{self.datetime}</b>\n\nOrangutan terdeteksi!\nGambar diambil oleh kamera {self.slave}\nConfidence: {round(self.confidence,2)*100}%\n\nCatatan: Perlu divalidasi kembali apakah benar orangutan atau bukan.\n\n© Balik Kampoeng 2024"
      else:
        return f"<b>{self.datetime}</b>\n\nTidak ada orangutan\nGambar diambil oleh kamera {self.slave}\n\n© Balik Kampoeng 2024"

    def _forward_image(self,image, caption):
      encoded_image = base64.b64encode(image).decode('utf-8')
      payload = {
        "image": encoded_image,
        "caption": caption
      }

      response = requests.post(self.lambda_url, json=payload)

    def _get_image(self,image_path):
      with open(image_path, 'rb') as image_file:
        image_data = image_file.read()
        return image_data

    def telegram_send(self, img_path, caption):
      here = os.path.dirname(os.path.realpath(__file__))
      os.environ['LAMBDA_URL'] = "https://4bqzrnujiseezufsycqckaab6i0ihvhj.lambda-url.ap-southeast-1.on.aws/"
      self.lambda_url = os.environ['LAMBDA_URL']

      image = self._get_image(img_path)
      self._forward_image(image, caption)

    def delete_files_in_directory(self,directory_path):
      # Check if the directory exists
      if not os.path.exists(directory_path):
          print(f"The directory {directory_path} does not exist.")
          return
      
      # Check if the directory path is indeed a directory
      if not os.path.isdir(directory_path):
          print(f"The path {directory_path} is not a directory.")
          return

      # Iterate over all the files in the directory
      for filename in os.listdir(directory_path):
          file_path = os.path.join(directory_path, filename)
          try:
              if os.path.isfile(file_path) or os.path.islink(file_path):
                  os.unlink(file_path)  # Remove the file or symbolic link
                  print(f"Deleted file: {file_path}")
              elif os.path.isdir(file_path):
                  print(f"Skipping directory: {file_path}")
          except Exception as e:
              print(f"Failed to delete {file_path}. Reason: {e}")
       

    def run_mode_1(self):
      """
      Received image, run the model prediction, and send the output to telegram
      Args:
      """

      result = self.predict(img_path=self.img_path, weights=self.weights)
      self.analyze_results(result)
      caption = self.make_caption()
      img_path = os.path.join("/home/trapcam/path_to_python_script/predicted_images",self.img_name)
      self.telegram_send(img_path, caption)
    
    def _generate_binary_string(self,combination, elements):
      return ''.join(['1' if elem in combination else '0' for elem in elements])

    def _check_active_camera(self, dirpath = None):
      """
      Check for active slaves
      """
      expected_slaves = [1, 2 ,3, 4]
      image_lists = os.listdir(dirpath)
      
      print("image list: ", image_lists)
      reported_slaves = sorted([int(x) for x in [re.search(r'.*_esp32-cam-(\d+)\.jpg', x).group(1) for x in image_lists]])
      print("reported slaves: ", reported_slaves)
      # Generate all possible combinations
      all_combinations = []
      for r in range(1, len(expected_slaves) + 1):
          combinations = itertools.combinations(expected_slaves, r)
          all_combinations.extend(combinations)

      # Convert each combination from tuple to list
      all_combinations = [list(comb) for comb in all_combinations]

      # Generate and print the binary strings for each combination
      binary_strings = [self._generate_binary_string(comb, expected_slaves) for comb in all_combinations]

      idx = all_combinations.index(reported_slaves)
      return binary_strings[idx]
    
    def run_mode_2(self):
      """
      MORNING TIME, BOOTING
      Received image from 4 images
      Args:
      """
      dirpath = self.morning_report
      caption = f"<b>[STATUS KAMERA PAGI HARI]</b>\nKamera telah aktif.\n{self.datetime}"

      active_camera = self._check_active_camera(dirpath)
      self.telegram_send(img_path=f"/home/trapcam/path_to_python_script/report_img/morning_{active_camera}.png", caption = caption)


    def run_mode_3(self):
      """
      EVENING TIME, CLOSING DOWN
      What this function do:
        - Clear up all images captured from that day
        - Report to telegram
      Args:
      """
      dirpath = self.night_report
      print("Dirpath: ",dirpath)
      caption = f"<b>[STATUS KAMERA MALAM HARI]</b>\nKamera telah non-aktif.\n{self.datetime}"
      active_camera = self._check_active_camera(dirpath)
      self.telegram_send(img_path=f"/home/trapcam/path_to_python_script/report_img/evening_{active_camera}.png", caption = caption)
      self.delete_files_in_directory(dirpath)


    def main(self):
      if self.mode == "1":
        self.run_mode_1()
      elif self.mode == "2":
        self.run_mode_2()
      elif self.mode == "3":
        self.run_mode_3()

if __name__ == "__main__":
  if len(sys.argv) != 4:
      print("Usage: python script.py <img_path> <predicted_img_name> <mode>")
      sys.exit(1)

  img_path = sys.argv[1]
  predicted_img_name = sys.argv[2]
  mode = sys.argv[3]

  handler = HandleMaster(img_path, predicted_img_name, mode)
  handler.main()
