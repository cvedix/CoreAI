import os

def replace_in_file(filepath, replacements):
    with open(filepath, 'r') as f:
        content = f.read()
    
    for old_str, new_str in replacements:
        content = content.replace(old_str, new_str)
        
    with open(filepath, 'w') as f:
        f.write(content)

def process_directory(directory, replacements):
    for root, _, files in os.walk(directory):
        for file in files:
            filepath = os.path.join(root, file)
            replace_in_file(filepath, replacements)

if __name__ == "__main__":
    # For YOLOv12
    process_directory('trt_yolov12', [
        ('trt_yolov11', 'trt_yolov12'),
        ('TRT_YOLOV11', 'TRT_YOLOV12'),
        ('yolov11', 'yolov12'),
        ('YOLOv11', 'YOLOv12'),
    ])
    
    # For RF-DETR
    process_directory('trt_rf_detr', [
        ('trt_yolov11', 'trt_rf_detr'),
        ('TRT_YOLOV11', 'TRT_RF_DETR'),
        ('yolov11', 'rf_detr'),
        ('YOLOv11', 'RF-DETR'),
        ('Yolov11', 'RfDetr'),
    ])
    print("Replacements completed.")
