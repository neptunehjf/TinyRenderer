
# Software Rasterizer

‌**概要**‌  
グラフィックスAPIを一切使わず、コンピュータグラフィックス理論と数学だけで実装したラスタライザー。

GPUではなくCPU上で動作するソフトウェアラスタライザー(Software Rasterizer)。

‌**技術スタック**‌

-   ‌**プログラミング言語**‌: C++
-   ‌**理論基盤**‌: グラフィックス学、数学、物理、信号画像処理
-   ‌**ツール**‌: Visual Studio2022、Photoshopなど

‌**プロジェクト構成**‌

-   ‌**Referrence**‌ - 参照資料（自分で数学式の導出プロセスも含む）
-   ‌**Lesson 0: Getting Started**‌ - TGA形式のグラフィック出力
-   ‌**Lesson 1: Bresenham’s Line Drawing Algorithm**‌ - ブレゼンハムの線描画アルゴリズム。各アルゴリズムの効率の比較。Bounding Box
-   ‌**Lesson 2: Triangle Rasterization and Back Face Culling**‌ - 三角形ラスタライゼーションおよびシンプルな面カリング
-   ‌**Lesson 3: Hidden Faces Removal (Z Buffer)** - 三角形ラスタライゼーションおよびシンプルな面カリング
-   ‌**Lesson 4: Perspective Projection**‌ - 透視投影
-   ‌**Lesson 5: Moving the Camera**‌ - カメラの移動制御
-   ‌**Lesson 6: Shaders for the Software Renderer**‌ - 頂点シェーダーとフラグメントシェーダーの実装
-   ‌**Lesson 6bis: Tangent Space Normal Mapping**‌ - 接空間法線マッピング
-   ‌**Lesson 7: Shadow Mapping**‌ - シャドウマッピング
-   ‌**Lesson 8: Ambient Occlusion**‌ - アンビエントオクルージョン

※ コード中のコメントは一応日本語化していますが、自分用のメモとして中国語のコメントも多く残っています。悪しからずご了承ください。

※ プロジェクトではgeometry、model、tgaimage三つのツールクラスについては、時間の都合上、自身で実装せず、チュートリアル提供のコードをそのまま流用しました。

‌**実行方法**‌  
-   画像が上書きされるのを防ぐため、実行前にプロジェクトディレクトリ内の既存画像を削除する必要があります
-   Visual Studio 2022で該当プロジェクトの`.sln`ファイルを開き、コンパイル後、VS 2022のデバッガーで実行します。

‌**成果画像の例**‌  
-  ブレゼンハムの線描画 
<img src="https://github.com/user-attachments/assets/f1409e7c-a07e-4cd8-96b6-cd293e05ec2e" width="600px" />
<br>
<br>
<br>

-  Phong Shader
<img src="https://github.com/user-attachments/assets/1245db56-67a8-4b16-afa5-80e58e60c745" width="600px" />
<br>
<br>
<br>

-  UVマッピングとシャドウマッピング
<img src="https://github.com/user-attachments/assets/28b003bf-7cfc-4d82-a07b-7d3bdfe5a6fc" width="600px" />
<br>
<br>
<br>

-  アンビエントオクルージョン
<img src="https://github.com/user-attachments/assets/4027e35c-b7fe-462c-8574-e2823f04de53" width="600px" />
<br>
<br>
<br>

‌**連絡先**‌  
koalahjf@gmail.com
