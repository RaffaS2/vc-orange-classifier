# vc-orange-classifier

This project is written in C/C++ and focuses on building a real-time orange quality classifier that processes video frames using low-level image processing operations, and blob tracking through inter-frame centroid matching.

---

## Requirements

- **g++** (C++11 or later)
- **OpenCV 4** — used for video capture and display only; all image processing is done from scratch

### Install OpenCV 4 (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install libopencv-dev pkg-config
```

### Install OpenCV 4 (macOS)

```bash
brew install opencv pkg-config
```

---

## Build

Clone the repository and navigate into the project folder:

```bash
git clone https://github.com/your-username/vc-orange-classifier.git
cd vc-orange-classifier
```

Build using the provided Makefile:

```bash
make
```

This produces a `vctrack` executable in the same directory.

To clean compiled objects and the binary:

```bash
make clean
```

---

## Run

Make sure `video.avi` is in the same directory as the executable, then run:

```bash
./vctrack
```

The program opens a window showing the video feed with real-time annotations:
- **Green bounding box** around each detected orange
- **Red dot** at the centroid
- **HUD overlay** with calibre, diameter (mm), area, perimeter, and quality category per orange
- **Frame counter** and running **total orange count** in the top-left corner

Press **`q`** to quit.

---

## Project Structure

```
vc-orange-classifier/
├── source.cpp       # Main loop — video capture, processing pipeline, display
├── functions.cpp    # All image processing functions (HSV, morphology, labelling, tracking)
├── functions.h      # Structs (IVC, OVC) and function declarations
├── makefile         # Build configuration
└── video.avi        # Input video (must be present at runtime)
```

---

## How It Works

1. Each BGR frame is converted to **HSV** color space
2. **HSV segmentation** isolates orange-colored pixels (H: 5–35°, S: 45–100%, V: 23–100%)
3. **Morphological opening** (erosion + dilation) removes noise from the binary mask
4. **Blob labelling** assigns a unique ID to each connected region
5. Each blob is **validated** against the CEE 379/71 regulation (minimum calibre, fill ratio ≥ 0.70, fully visible bounding box)
6. **Defect ratio** is computed once per orange and cached across frames using centroid proximity
7. **Inter-frame tracking** counts oranges as they exit the frame
