# Scientific Visualization - Volume Rendering

## 2021/11/14
* 修正讀取非 1:1:1 Ratio 的 Volume Data，在 Ray Casting 跟 ISO Surface 會有比例不對的問題。

## Future Work
* Post Processing by using framebuffer (edge detected or gaussian blur)
* Transfer function (UI need improve) => RGB (the knots) (Beizer Curve) [Developing]
* 讀取 inf 檔案 (新增支援 BigEndian)

## 不是很急，but easy to do...
* 調整Gradient的閥值，但是發現Heatmap的最大Y值變動很奇怪
* Add a point light struct in fragment shader
* Iso Surface 的顏色 => UI Control
* the culling plane (one normal vector and a position)
* Output the transfer function to a disk file (Cloud be more information)

## If i have some spare time, to do...
* Square => high frequencies with high opacities; low opacities for other table entries.
* Slicing Method
* View-Align or Axis-Align
* Back to front

![](https://i.imgur.com/Ke7oSoX.png)

![](https://i.imgur.com/nhUGzm4.png)

![](https://i.imgur.com/i7r4Nbl.png)

![](https://i.imgur.com/a5vuX1t.jpg)