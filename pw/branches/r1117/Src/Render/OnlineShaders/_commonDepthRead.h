// pixel shader input
struct VS_OUTPUT_DR
{
  float2 Texcoord : TEXCOORD0;
  float4 ZScale   : TEXCOORD1;
};


float4 madValueUV : register(VSHADER_LOCALCONST0);
float4 madValuePos : register(VSHADER_LOCALCONST1);
float4 uvScaleOffset : register(VSHADER_LOCALCONST2);
float4x4 ViewToTargetSpace : register(VSHADER_LOCALCONST3); // ViewInv * LightViewProj for shadows, VS2FogMapping for warfog


float4 VS_DepthReadParams(VS_INPUT Input, out float2 Texcoord, out float4 Position : POSITION)
{
  Position = float4(Input.Position.xy * madValuePos.xy + madValuePos.zw, 1.0f, 1.0f); 

  float2 texCoord = madValueUV.xy * Input.Texcoord + madValueUV.zw;
  Texcoord = texCoord;
  
  // float4 (w = 0), а НЕ float3: для mul(float3, float4x4) DXC собирает
  // матрицу из трёх столбцов через OpCompositeConstruct, а канонический фикс
  // раскладки (tools/shader_build/spirv_iface.py :: _flip_via_text) правит
  // ТОЛЬКО матрицы, пришедшие прямо из OpLoad из c-буфера. Для float3 операнд -
  // Construct, фикс его пропускает, и умножение остаётся транспонированным к
  // DXBC. Замер пары блобов FullScreenMaterial (2026-10-26): DXBC -
  // `dp3 o0.z, r0, c23` + `dp3 o0.w, r0, c24` (v . C_i = строки), SPIR-V -
  // OpMatrixTimesVector(mat4x3 из C_0..C_2, v) (v . C^T). Следствие для war
  // fog: warfogMapping уходит за [0,1], выборка карты 205x205 прижимается к её
  // туманному краю -> lerp(scene, RGB(30,53,96), 0.498) на весь кадр (у D3D9
  // пелены нет). С float4(w=0) DXC грузит матрицу целиком (OpLoad), фикс её
  // переворачивает, SPIR-V совпадает с DXBC; для D3D9-пути w=0 ничего не меняет
  // (c26 = (0,0,0,1), вклад 0).
  float4 zScale = float4( (texCoord * uvScaleOffset.xy + uvScaleOffset.zw), 1.0f, 0.0f );
  return mul(zScale, ViewToTargetSpace);
}
