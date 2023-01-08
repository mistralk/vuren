# 렌더패스, 파이프라인 추상화 개선 노트

2023-01-08

### 목표

- 기존의 지저분한 렌더 패스와 파이프라인 추상화를 좀더 개선

### 모티베이션

- 레이 트레이싱 렌더패스를 추가하려고 보니
- 레이 트레이싱 파이프라인도 추가해야하는데
- 파이프라인 종류에 따라 특정 함수와 프로퍼티를 사용할 수 없는 등 인터페이스가 매우 불편
- 렌더패스 및 파이프라인을 추가할때마다 바꿔야할게 너무 많음
- 그렇다고 모든걸 과도하게 추상화하는건 쓸데없이 프로그램 복잡도만 높임 (포인터, 생성자 처리 등등)

### 심플한 렌더패스 디자인 룰

- 렌더패스가 추상화된 파이프라인을 소유한다.
    - 그래픽스, 레이트레이싱, 컴퓨트
    - 오로지 하나의 파이프라인만 소유 가능하다.
- 렌더패스 그 자체는 더이상 추상화하지 않는다.
    - 이런 상속지옥은 보고싶지 않기 때문:
        - GBufferRasterRenderPass ← public RasterRenderPass ← public RenderPass
        - AwesomeRtRenderPass ← public RayTracingRenderPass ← public RenderPass
        - …
- 파이프라인 컨피규레이션 방법
    - 파이프라인의 타입에 따라 설정에 필요한 매개변수가 다르다.
    - 따라서 공통된 setupPipeline() 함수를 사용한다거나, 생성자에서 일일이 매개변수 넘겨줘서 처리하는건 번거롭다.
    - 사용자가 실수로 해당 렌더패스와 관계없는 잘못된 setupPipeline() 함수를 호출하는걸 쉽게 감지할 방법이 없다.

### 대안 : 컴포넌트화

- 렌더패스의 종류는 enum PipelineType을 통해 결정된다.
    - PipelineType은 렌더패스 생성자 호출시 사용자가 반드시 명시해줘야 함
    
```cpp
enum PipelineType {
    eRasterization,
    eRayTracing,
    eCompute
};

...

OffscreenRenderPass() : RenderPass(PipelineType::eRasterization) { ... }
GoodRtPass() : RenderPass(PipelineType::eRayTracing) { ... }
```
    
- 렌더패스는 파이프라인들이 필요한 모든 프로퍼티들을 구조체로, 일종의 “컴포넌트”로서 가지고 있다.
    - RasterProperties (e.g., vertex shader path, frag shader path, …)
    - RayTracingProperties (e.g., raygen shader path, miss shader path, blas, tlas, …)
    - ComputeProperties
- 즉 현재 렌더패스에선 필요없는 다른 타입의 파이프라인의 프로퍼티도 전부 갖고있다.
    - 예를들어 레이 트레이싱 렌더패스도 raster나 compute용 프로퍼티를 갖고는 있음
- 모든 렌더패스는 setup[Raster|Rt|Compute]Pipeline 함수를 갖고 있다.
    - 이 함수 안에서, 해당하는 파이프라인 객체를 make_unique로 생성하여 m_pPipeline 설정
    - 현재 패스와 관계없는 setup[Raster|Rt|Compute]Pipeline 함수를 호출하는 것도 가능은 하다.
    - 그러나 PipelineType을 통해서, 자신의 렌더패스 타입에 해당하지 않는 파이프라인을 설정하려고 하는 경우 런타임 assertion을 통해 실수 방지

이런 느낌:

```cpp
void RenderPass::setupRasterPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, bool isBlitPass) {
    assert(m_pipelineType == PipelineType::eRasterization);

    m_rasterProperties.vertShaderPath = vertShaderPath;
    m_rasterProperties.fragShaderPath = fragShaderPath;
    m_rasterProperties.isBiltPass = isBlitPass;

    m_pPipeline = std::make_unique<RasterizationPipeline>(m_pContext, m_renderPass, m_descriptorSetLayout, m_pipelineLayout, m_rasterProperties);

    m_pPipeline->setup();
}
```

### 장점

- 커스텀 렌더패스 생성할 때, 사용자는 PipelineType만 정의해주고 해당하는 setup 함수를 호출해주면 파이프라인 생성이나 포인터 핸들링을 생각할 필요가 없다.
- 실수로 잘못된 setup 함수를 호출하더라도 assertion으로 방어 가능
- 디버깅 피쳐나 렌더그래프을 도입한다고 했을 때, 현재 렌더패스의 타입이 무엇인지 쉽게 판별 가능

### 과제

- 이를테면 렌더패스마다 파이프라인 내의 세세한 파라미터들이 다를 수 있는데, 이 경우는 어떻게 깔끔하게 처리할지?