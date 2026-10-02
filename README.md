# OsteoSense

A wearable knee patch that tries to catch early signs of osteoarthritis (OA) without X-rays or MRI.

Built by **Blue Code Crew** for Smart India Hackathon 2026
Problem statement: SIH26004, AI-assisted early detection of OA risk markers in the North Eastern Region
Category: Hardware / MedTech

> **Status:** working prototype on a bench. This is a screening idea, not a certified medical device, and nothing here should be used to diagnose anyone. Clinical validation hasn't been done yet.

---

## Why we're doing this

Osteoarthritis is usually found late, when cartilage is already worn down and an X-ray shows it. By then the damage is done. Several studies point to the infrapatellar fat pad (Hoffa's fat pad, HFP) as an early marker: it changes before the cartilage visibly does.

In the North East, the people most at risk (tea garden workers, hill communities with long, steep walks) are also the people furthest from an MRI machine. We wanted something cheap, radiation-free and simple enough for a frontline health worker to place on a knee.

## How it works

Two sensors look at the same spot, just below the kneecap:

- **Near-infrared (NIR) sensing** picks up changes in the fat pad, such as fluid content.
- **Bio-impedance** measures how the tissue resists a small electrical signal, which shifts as tissue breaks down.

An ESP32 reads both, sends the data over Wi-Fi to Firebase, and a web dashboard shows the results. The patch also shows a live index on its own display, so it's useful without a phone.

The readings are combined into a pre-symptomatic index. Green means advise exercise and lifestyle changes; amber or red means refer for imaging at a district hospital.

```
 knee patch (ESP32 + sensors)
        |
        |  Wi-Fi
        v
     Firebase  --->  web dashboard
        |
        v
  index / report / referral flag
```

If there's no connection (common in remote areas), readings are stored on the device and synced later.

## Hardware

| Part | What it does |
|------|--------------|
| ESP32 DevKit V1 | Main controller, Wi-Fi |
| AS726x | NIR / spectral sensing of the fat pad |
| AD5933 | Impedance converter for the bio-impedance readings |
| MAX30102 | Used early on to test the signal pipeline before moving to the other sensors |
| Small display | Shows the live index on the patch |

Power is 5V over USB for now; the sensors run off the 3.3V rail. Pin assignments are at the top of the firmware sketch, so check them against your wiring before flashing.

## Repo layout

```
firmware/     Arduino sketches (C++) for the ESP32
dashboard/    web interface
docs/         wiring diagrams, notes, references
```

## Getting it running

You'll need the Arduino IDE with ESP32 board support installed.

1. Clone the repo.
2. Open the sketch in `firmware/` in the Arduino IDE.
3. Install the libraries for the AS726x and AD5933 from the Library Manager.
4. Copy the config file and fill in your own Wi-Fi and Firebase details. **Don't commit these.**
5. Select the ESP32 Dev Module board, pick the right port, and upload.
6. Open the serial monitor at the baud rate set in the sketch to check that readings are coming through.
7. Open the dashboard and log in.

## Known problems

Being straight about these:

- The HFP degradation index comes from published research and hasn't been commercialised, so we don't have a ready-made reference to calibrate against.
- Placement matters a lot. The fat pad has to be located with the patient sitting, and getting it right takes some practice.
- We need labelled data from clinicians before the AI comparison step means much. Until then the index is indicative only.

## Plan

1. Bench-test against healthy and OA proxy samples.
2. Work with clinicians to get properly labelled data.
3. Improve firmware and signal quality iteratively.
4. Field trials with ASHA/ANM workers, with voice prompts in local languages (Assamese, Bengali, Nepali, Bodo, Khasi, Mizo, Manipuri).

## References

- PMC11972505
- PMID 29648688
- Inflammatory Research journal, vol. 18-25
- Datasheets: ESP32, AD5933, AS726x

## Contact

Blue Code Crew, bluecodecrew@gmail.com