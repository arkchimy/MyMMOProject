struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer CB : register(b0)
  {
      matrix transform;
      float2 uvOffset;
      float2 uvScale;
  };

VSOutput main(VSInput vsInput)
{
    VSOutput vsOutput;

    vsOutput.position = mul(vsInput.position, transform);
    vsOutput.uv = vsInput.uv * uvScale + uvOffset;  // 프레임 위치로 변환

    return vsOutput;
}
