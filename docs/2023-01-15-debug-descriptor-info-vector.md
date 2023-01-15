# std::vector\<WriteDescriptorSet\> 디버깅 노트

2023-01-15

- 현재 코드는 1 바인딩 = 1 텍스쳐만 할당 가능하도록 되어있다. 
- 셰이더에서 바인딩 하나로 여러개의 텍스쳐에 액세스할 수 있도록 바꾸고 싶다. 
  - 특히 레이 트레이싱 셰이더에서는 씬 전체의 텍스쳐, 매터리얼 버퍼에 전부 접근할 수 있어야 좋을 것 같다.
  - bindless도 고려 대상이지만 나중에
  - 디스크립터 인덱싱
    - https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html
    - https://github.com/SaschaWillems/Vulkan/blob/master/examples/descriptorindexing/descriptorindexing.cpp
- 현재 단일 바인딩의 디스크립터 카운트를 무조건 1로 설정하고 있으므로 이 부분을 수정해야 함
- 우선은 지저분한 descriptor set update 코드를 리팩토링부터 해야겠다.

### 버그

- 리팩토링 했더니 write descriptor info가 제대로 쓰이지 않는 괴현상이 발생
- 신기하게도 래스터 패스에선 발생하지 않고, 레이 트레이싱 패스에서만 발생함
- 혹시 DescriptorBufferInfo의 프로퍼티에 내가 뭔가 오해한 정의가 있는게 아닌가 다시 한번 확인
  - `VkDescriptorBufferInfo`
    - buffer: 버퍼 리소스 핸들
    - offset(in bytes): buffer의 시작점으로부터의 오프셋. 해당 디스크립터를 통해 메모리에 접근할때 주소는 이 베이스 오프셋을 기준으로 계산됨
    - range(in bytes): 디스크립터 업데이트에 사용되는 크기. 또는 offset ~ 버퍼 끝까지 VK_WHOLE_SIZE를 이용할 수 있음
  - 다 제대로 넣었다.
- 삽질하며 디버깅한 결과, 첫번째 버퍼 디스크립터 인포를 처음 추가할 때는 멀쩡하다가, 두번째 버퍼 디스크립터 인포를 추가하자마자 첫번째의 bufferInfo.offset이 쓰레기 값으로 바뀌어버리고 있음을 발견
  - 실제로 래스터/레이 트레이싱 여부는 관계없었음 (그냥 여러 조건이 겹친 우연의 일치였다)

```cpp
case vk::DescriptorType::eUniformBuffer: // first buffer
  bufferInfos.push_back(bufferInfo);
  write.pBufferInfo = &bufferInfos.back();
...
case vk::DescriptorType::eStorageBuffer: // second buffer
  bufferInfos.push_back(bufferInfo);
  write.pBufferInfo = &bufferInfos.back(); // 여기서 갑자기 첫번째 버퍼 정보도 바뀐다
```

- 어떻게 `push_back` 한다고 내용물이 바뀌지?
- 곰곰히 생각해보니 처음에 벡터 사이즈는 0으로 초기화 될 것. 그리고 새로 원소가 추가되면서 **다른 주소로 재할당**이 발생할 것이다.
  - 리팩토링 전에는 WriteDescriptorSet 벡터를 미리 resize 해줘서 에러가 없었던 것
- 어째서 range는 변하지 않고 offset만 바뀌었는지에 대해선 std::vector의 재할당 방식을 확인해봐야
- 어쨌든 전체 디스크립터 개수만큼 넉넉하게 bufferInfos 벡터, imageInfos 벡터를 미리 reserve해두는 것으로 해결
- Vulkan과 전혀 관계없는 실수였다.