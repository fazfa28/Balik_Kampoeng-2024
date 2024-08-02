from ultralytics import YOLO
from PIL import Image
import os as os
import requests
import base64
import os
import sys


#SELECT PRETRAINED MODEL IN CURRENT DIRECTORY
model = YOLO("/home/trapcam/path_to_python_script/best.pt")

#LOAD TEST IMAGES
if len(sys.argv) < 2:
   sys.exit(1)

image_path = sys.argv[1]
print(image_path)
predicted_image_name= sys.argv[2] 

results = model(image_path)

#USE VERBOSE TO INFER DETECTION OF ORANGUTAN
def analyze_results():
    detection_string = results[0].verbose()
    if "no detections" in detection_string:
        print(results[0].path, "doesn't contain orangutan")
    else:
        # if(r.boxes.conf >= 0.5)
        print(results[0].path, "contains orang utan")
        #else:


#PRINTING THE RESULTS
#analyze_results()


def show_results():

    # Plot results image
    im_bgr = results[0].plot()  # BGR-order numpy array
    im_rgb = Image.fromarray(im_bgr[..., ::-1])  # RGB-order PIL image
    try:
        float_tensor = float(results[0].boxes.conf[0].item())
        print(float_tensor)

        # Save results to disk
        results[0].save(filename=f"/home/trapcam/path_to_python_script/predicted_images/{predicted_image_name}")
        return round(float_tensor, 2)
    except:
        results[0].save(filename=f"/home/trapcam/path_to_python_script/predicted_images/{predicted_image_name}")
        return 0.00

#TELEGRAM SEND

here = os.path.dirname(os.path.realpath(__file__))
os.environ['LAMBDA_URL'] = "https://4bqzrnujiseezufsycqckaab6i0ihvhj.lambda-url.ap-southeast-1.on.aws/"
lambda_url = os.environ['LAMBDA_URL']

# Input: bytes
def forward_image(image, caption):
  encoded_image = base64.b64encode(image).decode('utf-8')
  payload = {
    "image": encoded_image,
    "caption": caption
  }

  response = requests.post(lambda_url, json=payload)
  print(response.json())

# SAMPLE USAGE:
def get_image(image_path):
  with open(image_path, 'rb') as image_file:
    image_data = image_file.read()
    return image_data

#show the probability the picture contains orangutan
res = "Test slave YOLO in raspi: "+ str(show_results()) 

forward_image(get_image(os.path.join("/home/trapcam/path_to_python_script/predicted_images",predicted_image_name)), res)

    
