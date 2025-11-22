# SAPS: Selective Automated Pesticide Sprayer

**Institution:** Indian Institute of Information Technology Design and Manufacturing Kurnool

## 📖 Abstract
In the pursuit of sustainable agriculture, **SAPS (Selective Automated Pesticide Sprayer)** was developed to reduce chemical waste, minimize environmental impact, and optimize pesticide usage. The system is built around an **ESP32-CAM** module for real-time image capture and an **Arduino** unit that controls a motor-driven spray mechanism. By utilizing AI-driven detection, SAPS identifies crops with a 70% confidence threshold and applies pesticide solely to targeted plants, ensuring precision and reducing excessive chemical use.

## 🚀 Key Features
* **AI-Powered Detection:** Utilizes Edge AI on the ESP32-CAM to detect crops and plant health in real-time with 91% accuracy.
* **Precision Spraying:** Activates the spray mechanism only when a target crop is identified with >70% confidence.
* **Intermittent Movement:** Operates on a cycle (0.5s movement, 4s pause) to ensure stable image capture and accurate targeting.
* **Cost-Effective:** Reduces dependency on expensive GPS-guided systems by using affordable microcontrollers.
* **Eco-Friendly:** Achieved a 35-40% reduction in pesticide usage compared to traditional methods.

## 🛠️ Hardware Components
The system utilizes the following hardware:
* **Microcontroller:** Arduino Uno (Logic & Control).
* **Image Processing:** ESP32-CAM Module.
* **Actuators:**
    * DC Motors (for movement).
    * Submersible Water Pump (for spraying).
* **Drivers:** L298N Motor Driver.
* **Power:** 12V Battery (Motors) & 5V Regulator (Logic).
* **Indicators:** LED (Visual feedback for active spraying).

## 💻 Software & Tech Stack
* **Language:** C++ (Arduino IDE).
* **AI/ML:** Edge AI (TensorFlow Lite for Microcontrollers) trained on custom crop datasets.
* **Communication:** Binary signal conversion between ESP32-CAM and Arduino.

## ⚙️ Working Principle
1.  **Movement:** The robot moves forward for **0.5 seconds**.
2.  **Capture:** The system pauses for **4 seconds**; the ESP32-CAM captures an image.
3.  **Inference:** The AI model processes the image. If a crop is detected with **≥70% confidence**, it sends a HIGH signal to the Arduino.
4.  **Action:** The Arduino triggers the relay/pump to spray and blinks the LED indicator.
5.  **Loop:** The cycle repeats.

## 📊 Results and Observations
* **Accuracy of Leaf Detection:** The AI model achieved **91% accuracy** in detecting crops during testing, demonstrating strong capability in distinguishing targets from the environment.
* **Efficiency of Pesticide Spraying:** The system achieved a **35-40% reduction** in pesticide use compared to traditional methods.
* **Challenges & Solutions:**
    * **Real-Time Processing:** The model was optimized for lower memory usage to run efficiently on the limited RAM of the ESP32-CAM.
    * **Mechanical Synchronization:** Motor timing was adjusted to strictly align the 0.5-second movement with the spray activation to prevent misalignment.

## 👥 Team Members
| Name | Roll No |
| :--- | :--- |
| Sankalp Thorat | 124CS0049 |
| Aniket Patil | 124CS0081 |
| Neelima | 124CS0058 |
| Sahithi | 524CS0013 |

## 🔮 Future Scope
* **Solar-Powered Operation:** Integrating solar power to enable continuous operation without frequent recharging, making the system ideal for off-grid or large-scale farms.
* **Multi-Crop Detection:** Expanding the AI model to detect multiple crop types and weeds to enhance versatility across different farming environments.
* **IoT Integration:** Adding IoT features to allow farmers to monitor and control the system remotely, providing real-time data for better decision-making.
