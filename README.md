# CLAHE
 
## 2D CLAHE
C++ implementation on the CPU of Contrast Limated Adaptive Histogram Equalization 
based on the original method presented in the paper
"Contrast Limited Adaptive Histogram Equalization" by Karel Zuiderveld, 
in "Graphics Gems IV", Academic Press, 1994

## 3D CLAHE
C++ implementation on both the CPU and GPU of 3D Contrast Limated Adaptive Histogram Equalization 
based on the method presented in the paper 
["Adaptive Histogram Equalization Method for Medical Volumes"](https://pdfs.semanticscholar.org/200d/8c75564578aeadc34a1114f7f687ccf9b372.pdf)
 by Paulo Amorim, Thiago Moraes, Jorge Silva and Helio Pedrini
GPU implementation is using GLSL Compute Shaders

## Focused CLAHE
Focused CLAHE applies the CLAHE algorithm to a specified section within the image or volume. A 2D Focused CLAHE is implemented in C++, and a 3D Focused CLAHE is implemented on both the CPU and GPU. 


## 2D CLAHE Results
#### Changing the number of Contextual Regions
| <img src="https://github.com/klucknav/Images/blob/master/dicomTest_og.jpg" align="middle" width="200px"/> | <img src="https://github.com/klucknav/Images/blob/master/2x2_c85.jpg" align="middle" width="200px"/> | <img src="https://github.com/klucknav/Images/blob/master/4x4_c85.jpg" align="middle" width="200px"/> | <img src="https://github.com/klucknav/Images/blob/master/8x8_c85.jpg" align="middle" width="200px"/> |
|-----------|-----------|-----------|-----------|
| raw DICOM | 2x2 CR, 0.85 Contrast | 4x4 CR, 0.85 Contrast | 8x8 CR, 0.85 Contrast |

#### Changing the Clip Limit/Contrast
| <img src="https://github.com/klucknav/Images/blob/master/dicomTest_og.jpg" align="middle" width="200px"/> | <img src="https://github.com/klucknav/Images/blob/master/4x4_c50.jpg" align="middle" width="200px"/> | <img src="https://github.com/klucknav/Images/blob/master/4x4_c100.jpg" align="middle" width="200px"/> |
|-----------|-----------|-----------|
| raw DICOM | 4x4 CR, 0.50 Contrast | 4x4 CR, 1.0 Contrast |

#### Focused CLAHE
| <img src="https://github.com/klucknav/Images/blob/master/dicomTest_og.jpg" align="middle" width="200px"/> | <img src="https://github.com/klucknav/Images/blob/master/CLAHE/s_bottom_test.png" align="middle" width="200px"/> | <img src="https://github.com/klucknav/Images/blob/master/CLAHE/section_2x2.png" align="middle" width="200px"/> |<img src="https://github.com/klucknav/Images/blob/master/CLAHE/section_4x4.png" align="middle" width="200px"/> |
|-----------|-----------|-----------|-----------|
| raw DICOM | Focused CLAHE | Focused CLAHE 2x2 CR | Focused CLAHE 4x4 CR |

## Controls
Focused CLAHE:
* move the focused region within the volume
* change the dimensions of the focused volume
* change the clip Limit to adjust the contrast
