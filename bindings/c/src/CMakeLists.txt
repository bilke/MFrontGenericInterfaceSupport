mgis_library(MFrontGenericInterface-c SHARED
  Status.cxx
  ThreadPool.cxx
  Hypothesis.cxx
  Variable.cxx
  Behaviour.cxx
  State.cxx
  BehaviourData.cxx
  BehaviourDataView.cxx
  MaterialStateManager.cxx
  MaterialDataManager.cxx
  Integrate.cxx
  Model.cxx)
target_include_directories(MFrontGenericInterface-c
  PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/bindings/c/include>
  PUBLIC $<INSTALL_INTERFACE:include>)
target_link_libraries(MFrontGenericInterface-c
   PRIVATE MFrontGenericInterface)
