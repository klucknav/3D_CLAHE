# 3D CLAHE
Viewer of 3D Dicom Volumes and Real-time implementation of 3D Contrast Limated Adaptive Histogram Equalization 
based on the method presented in the paper ["Adaptive Histogram Equalization Method for Medical Volumes"](https://pdfs.semanticscholar.org/200d/8c75564578aeadc34a1114f7f687ccf9b372.pdf) by Paulo Amorim, Thiago Moraes, Jorge Silva and Helio Pedrini. It is implemented in on the GPU using GLSL Compute Shaders.
 
Two extensions are also included Focused CLAHE and Masked CLAHE.

## 3D CLAHE
3D CLAHE takes as input the number of SubBlocks, and the ClipLimit. CLAHE divides the volume into SubBlocks and performs Histogram Equalization on each of those blocks individually, this parameter adjusts the number of SubBlcks to use. The ClipLimit is a value between [0,1] the larger the value the more contrast in the final volume. A ClipLimit of 0 returns the original volume. 

## Focused CLAHE
Focused CLAHE applies the CLAHE algorithm to a specified section within the image or volume. It's inputs are the min and max 3D values to apply the CLAHE algorithm to, as well as the ClipLimit. 

## Masked CLAHE
Masked CLAHE applies the CLAHE algorithm to specific organs within the DICOM volume. The masked volume has an image for each slice in the corresponding DICOM volume. Each organ is lebeled with colors that are powers of 2. Each organ is it's own "SubBlock" and the adjustable parameter is the ClipLimit. 

## Keyboard Controls
| Key | Control |
|:---:|:-----------------------------------------------------------------------------------------------------------------------------:|
|  D  | View the raw DICOM Volume |
|  C  | View the 3D CLAHE Volume |
|  F  | View the Focused CLAHE Volume |
|  M  | View the Masked CLAHE Volume |
|  O  | View just the Masked Organs in the Volume |
| +/- | increase/decrease the clipLimit |
| S/s | increase/decrease the number of Sub-Blocks for 3D CLAHE<br>increase/decrease the number of pixels per Sub-Block for Focused CLAHE |
| X/x | Move the Focused Region in the +/- x direction<br>increase/decrease the x dimensions of the Focused Region |
| Y/y | Move the Focused Region in the +/- y-direction<br>increase/decrease the y dimensions of the Focused Region |
| Z/z | Move the Focused Region in the +/- z direction<br>increase/decrease the z dimensions of the Focused Region |

## Results 
| raw DICOM and 3D CLAHE |
| :-----: | 
| <img src="https://github.com/klucknav/Images/blob/master/CLAHE/3DCLAHE.png" align="middle"/> |

| Focused CLAHE | 
| :-----: | 
| <img src="https://github.com/klucknav/Images/blob/master/CLAHE/3DFocusedCLAHE.png" align="middle"/> |

| Masked CLAHE | 
| :-----: | 
| <img src="https://github.com/klucknav/Images/blob/master/CLAHE/MaskedCLAHE.png" align="middle"/> |
