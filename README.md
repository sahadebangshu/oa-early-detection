#  OsteoSense
### AI-Powered Multimodal Smart Knee Patch for Early Osteoarthritis Risk Assessment

> **One Patch. Multiple Signals. One AI-Based Risk Profile.**

OsteoTrack is a proposed wearable, non-invasive knee-monitoring system designed to explore **early risk assessment of Osteoarthritis (OA)** using multimodal physiological sensing and Artificial Intelligence.

The system combines **Bioimpedance Spectroscopy** and **Near-Infrared Spectroscopy (NIRS)** in a compact knee-worn patch. The acquired signals are processed using an **ESP32**, transmitted wirelessly through **Bluetooth**, and analysed using an AI/ML pipeline to generate an interpretable **OA risk profile** on a mobile application.

---

## 🚨 Problem Statement

Osteoarthritis is a progressive joint disorder in which structural and biochemical changes can occur before severe symptoms become obvious.

Current assessment pathways may involve:

- Clinical examination
- Patient history
- Imaging
- Laboratory investigations when required
- Specialist consultation

These approaches can be time-consuming, expensive, or dependent on clinical infrastructure.

### The challenge

**Can we develop a portable and affordable system that can continuously/rapidly capture complementary knee-tissue signals and use AI to identify patterns associated with early OA risk?**

---

# 💡 Our Solution

**OsteoTrack** proposes a smart knee patch capable of collecting complementary physiological information from the knee region.

Instead of relying on a single measurement, OsteoTrack combines:

### 🔵 Bioimpedance — AD5933
Measures electrical impedance characteristics of tissue across frequencies.

Potentially useful features include:

- Resistance
- Reactance
- Impedance magnitude
- Phase response
- Frequency-dependent impedance patterns

### 🟢 NIRS — AS7265x
Uses optical spectral information to investigate tissue-related changes.

Potential features include:

- Spectral response
- Wavelength-dependent features
- Optical signal patterns

The complementary signals are then processed and fused before AI-based analysis.

---

# 🏗️ System Architecture
<img width="1536" height="1024" alt="OA_system architechture" src="https://github.com/user-attachments/assets/78a10bb9-308f-4dbf-8139-db6a45d3290d" />
How It Works
1. Wear

The OsteoSense patch is positioned around the knee.

2. Sense

Bioimpedance and NIRS sensors acquire complementary signals from the knee region.

3. Process

The ESP32 receives and processes the raw sensor data.

Processing includes:

</p>-> Signal acquisitionv</p>
</p>-> Noise filtering</p>
</p>-> Artefact reduction</p>
</p>-> Feature extraction</p>
</p>-> Feature fusion</p>
4. Analyse

The extracted features are supplied to an AI/ML model.

The model is designed to learn relationships between multimodal signal patterns rather than depending on a single sensor.

5. Assess

The system generates an OA risk profile such as:

-> Low Risk
-> Moderate Risk
-> High Risk
6. Display

The result is transmitted through Bluetooth to a mobile application.

The application can display:

-> OA risk score
-> Signal quality
-> Key insights
-> Historical trends
-> Suggested next steps
## AI/ML Pipeline
Raw Sensor Signals
        ↓
Signal Pre-processing
        ↓
Feature Extraction
        ↓
Feature Normalisation
        ↓
Multimodal Feature Fusion
        ↓
AI / ML Model
        ↓
Risk Classification
        ↓
OA Risk Profile
### Why multimodal AI?

Different sensors provide different types of information.

Bioimpedance provides information related to **electrical tissue properties**, while NIRS provides **optical spectral information**.

Combining these complementary signals may allow the model to identify patterns that are not visible from a single signal alone.
## Mobile Application
## Hardware Components
| Component              | Purpose                                  |
| ---------------------- | ---------------------------------------- |
| **ESP32**              | Main microcontroller and processing unit |
| **AD5933**             | Bioimpedance measurement                 |
| **AS7265x**            | NIRS / spectral sensing                  |
| **Electrodes**         | Bioimpedance signal acquisition          |
| **Battery**            | Portable power source                    |
| **Charging Circuit**   | Battery charging                         |
| **Bluetooth**          | Wireless communication                   |
| **Knee Patch / Strap** | Wearable sensor integration              |
## Software Stack
**Embedded System**
-> ESP32
-> Sensor interfacing
-> Signal acquisition
-> Digital signal processing
-> Bluetooth communication
**AI / Machine Learning**

Potential pipeline:

-> Python
-> NumPy
-> Pandas
-> SciPy
-> Scikit-learn
-> TensorFlow / PyTorch (depending on final model)
**Mobile Application**

The mobile application can be implemented using a suitable framework such as:

*Flutter
*React Native
*Android/Kotlin
The final software stack may change during development.
## Prototype Cost

Our current target is:

**₹3,000 – ₹4,000**
Estimated proof-of-concept prototype cost

The target includes the major hardware required for:

*Multimodal sensing
*ESP32-based processing
*Wireless communication
*Wearable integration
*Battery-powered operation

The ₹3K–₹4K figure represents an estimated prototype cost, not a final commercial or clinical-device price.
<img width="1600" height="800" alt="oa_prototype" src="https://github.com/user-attachments/assets/2460683f-a632-4dee-99d8-1bda49f0d108" />
## Key Features
🦵 Wearable knee patch
🔬 Multimodal sensing
🔵 Bioimpedance-based sensing
🟢 NIRS-based sensing
🤖 AI/ML-based risk assessment
⚡ ESP32-based processing
📡 Bluetooth connectivity
📱 Mobile risk dashboard
💰 Low-cost prototype target
🔋 Portable operation
🚫 Non-invasive sensing approach
📊 Potential longitudinal monitoring
<img width="714" height="480" alt="oa_key feature" src="https://github.com/user-attachments/assets/dbfbe880-82b2-4a7c-9363-1d03cc5407ec" />
# What Makes OsteoSense Different?
**1. Multimodal Instead of Single-Sensor**

Rather than depending on one signal, OsteoTrack combines two complementary sensing modalities.

**2. Wearable**

The sensing system is designed around a knee-worn form factor.

**3. AI-Based Data Fusion**

The objective is to allow AI to learn patterns across multiple physiological signals.

**4. Portable**

The system is designed to reduce dependence on large clinical equipment for the sensing component.

**5. Cost-Conscious**

The proof-of-concept prototype is targeted at approximately ₹3K–₹4K.
## Limitations

OsteoSense is currently a prototype/research concept and has important limitations.

*Clinical validation is required.
*Sensor placement can influence measurements.
*Skin contact and environmental conditions may affect signals.
*AI performance depends on the quality and diversity of the dataset.
*False positives and false negatives are possible.
*Larger datasets are required for robust model training.
*The system is not intended to replace professional medical diagnosis.

## Future Scope

Future versions of OsteoTrack could include:

**Hardware**
*Custom miniaturised PCB
*Improved electrode design
*Better mechanical integration
*Lower-power electronics
*Improved sensor placement
**AI**
*Larger clinically labelled datasets
*Personalised baseline modelling
*Longitudinal risk tracking
*Explainable AI
*Improved multimodal fusion algorithms
**Application**
*Doctor/clinician dashboard
*Cloud-based data synchronisation
*Long-term monitoring
*Automated reports
*Remote screening support
**Validation**

Future development would require controlled studies and clinical validation against established OA assessment methods.
PROTOTYPE
    ↓
Sensor Calibration
    ↓
Signal Quality Testing
    ↓
Pilot Data Collection
    ↓
Feature Engineering
    ↓
AI Model Training
    ↓
Cross-Validation
    ↓
Clinical Dataset Validation
    ↓
Prototype Refinement
    ↓
Clinical Validation
## Target Impact

OsteoTrack aims to contribute towards a future where knee-health monitoring can become:

**Portable → Affordable → Data-driven → Accessible**

The long-term vision is to support earlier identification of OA-related risk patterns, encouraging timely professional evaluation and preventive management.
## Hackathon Vision

*“Move from late-stage detection to early-stage insight.”*

OsteoTrack combines wearable sensing, embedded systems, wireless communication and AI to create a potential new pathway for accessible knee-health monitoring.
## Team

Team OsteoSense

Developed as a prototype for Smart India Hackathon 2026.

**Team Members:**
</p>Pritam Paul</p>
</p>Debangshu Saha</p>
</p>Prantik Ghosh</p>
</p>Barnovo Kundu</p>
</p>Samaydip Roy</p>
</p>Adrija Chanda</p>

## Project Status
# Current Stage

*🟡 Prototype / Proof of Concept*
| Module                   | Status         |
| ------------------------ | -------------- |
| System Architecture      | 🟢 Designed    |
| Bioimpedance Integration | 🟡 Development |
| NIRS Integration         | 🟡 Development |
| ESP32 Processing         | 🟡 Development |
| Bluetooth Communication  | 🟡 Development |
| AI/ML Pipeline           | 🟡 Development |
| Mobile Application       | 🟡 Development |
| Clinical Validation      | 🔴 Future Work |
## Disclaimer

OsteoSense is a *research and prototype project.*

It is not currently a clinically validated diagnostic device and should not be used to diagnose, treat, or rule out Osteoarthritis. Any clinical application would require appropriate validation, regulatory compliance, and evaluation by qualified healthcare professionals.
