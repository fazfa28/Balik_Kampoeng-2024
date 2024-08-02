#!/bin/bash

sudo docker rm ultralytics_container

# Function to check if a Docker container is running
is_container_running() {
    sudo docker ps -q --filter "name=$1"
}

# Check if the Docker image already exists locally
IMAGE_EXISTS=$(sudo docker images -q ultralytics/ultralytics:latest-arm64)

# If IMAGE_EXISTS is empty, pull the Docker image
if [ -z "$IMAGE_EXISTS" ]; then
    echo "Docker image not found locally. Pulling the latest image."
    sudo docker pull ultralytics/ultralytics:latest-arm64
else
    echo "Docker image already exists locally. Skipping pull."
fi

CONTAINER_NAME="ultralytics_container"

# Check if the container is already running
RUNNING_CONTAINER=$(is_container_running "$CONTAINER_NAME")

# If RUNNING_CONTAINER is not empty, stop and remove the existing container
if [ -n "$RUNNING_CONTAINER" ]; then
    echo "Stopping the currently running Docker container."
    sudo docker stop "$RUNNING_CONTAINER"
    sudo docker rm "$RUNNING_CONTAINER"
fi

# Start a new Docker container
echo "Starting a new Docker container."
CONTAINER_ID=$(sudo docker run --name "$CONTAINER_NAME" -t -d --ipc=host \
    -v "$(pwd):/usr/src/app" \
    -v "/home/trapcam:/home/trapcam" \
    -v "/var/www/html/uploads:/var/www/html/uploads" \
    ultralytics/ultralytics:latest-arm64)

# Check if container ID is empty (indicating failure to start)
if [ -z "$CONTAINER_ID" ]; then
    echo "Failed to start Docker container."
    exit 1
fi

# Ensure the container is open
sleep 5

# Define boolean for first image
night_bool=1
morning_bool=1
report=1

# Define file paths and directories
last_image_file="/home/trapcam/image_temp/last_image.txt"
new_image_dir="/var/www/html/uploads"
album="/home/trapcam/album"
morning_report="/home/trapcam/morning_report"
night_report="/home/trapcam/night_report"

# Infinite loop to monitor new images
while true; do
  #current_time=$(date +"%H")
  current_time=7
  # Get the most recently modified .jpg file in the new_image_dir directory
  new_image=$(ls -t "$new_image_dir"/*.jpg 2> /dev/null | tail -n 1)
  # Read the last processed image from the last_image_file
  last_image=$(cat "$last_image_file" 2> /dev/null)

  # If there is a new image and it's different from the last processed image
  if [ -n "$new_image" ] && [ "$new_image" != "$last_image" ]; then
      new_image_name=$(basename "$new_image")
      echo "Latest image received: $new_image_name. Moving it to album folder"
      # Update the last_image_file with the new image path
      echo "$new_image" > "$last_image_file"
      echo "New image name: $new_image_name"
      
      if [ $current_time -gt 5 ] && [ $current_time -lt 18 ] && [ $morning_bool -eq 0 ]; then
          echo "Go to normal mode" 
          # Pass both full path and file name to the Python script inside the Docker container
          sudo docker exec "$CONTAINER_ID" python3 /home/trapcam/path_to_python_script/master_code_final.py "$new_image" "$new_image_name" "1"
          # Move the new image to the album directory
          mv "$new_image" "$album/$new_image_name"
    
      elif [ $morning_bool -eq 1 ] && [ $current_time -gt 5 ] && [ $current_time -lt 18 ]; then
          echo "Morning report mode"
          night_bool=1
          mv "$new_image" "$morning_report"
      else
          echo "Night report mode"
          morning_bool=1
          mv "$new_image" "$night_report"
      fi 
  else
    echo "No new image. Last image: $(basename "$last_image")"
  fi

  if [ $current_time -eq 19 ] && [ $night_bool -eq 1 ]; then
      echo "running script 3"
      sudo docker exec "$CONTAINER_ID" python3 /home/trapcam/path_to_python_script/master_code_final.py "$new_image" "$new_image_name" "3"
      night_bool=0
  fi 
  
  if [ $current_time -eq 7 ] && [ $morning_bool -eq 1 ]; then 
      sudo docker exec "$CONTAINER_ID" python3 /home/trapcam/path_to_python_script/master_code_final.py "$new_image" "$new_image_name" "2" 
      morning_bool=0
  fi 

  sleep 2  # Sleep for 2 seconds before checking again
done
