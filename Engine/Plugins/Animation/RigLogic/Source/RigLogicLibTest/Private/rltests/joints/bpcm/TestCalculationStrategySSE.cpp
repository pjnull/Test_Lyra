// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/system/simd/Detect.h"

#ifdef RL_BUILD_WITH_SSE

#include "rltests/Defs.h"
#include "rltests/controls/ControlFixtures.h"
#include "rltests/joints/bpcm/Helpers.h"
#include "rltests/joints/bpcm/BPCMFixturesBlock4.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/bpcm/Evaluator.h"
#include "riglogic/joints/bpcm/strategies/SSE.h"

namespace {

class SSEJointCalculationStrategyTest : public ::testing::TestWithParam<StrategyTestParams> {
    protected:
        void SetUp() {
            strategy = pma::UniqueInstance<rl4::bpcm::SSEJointCalculationStrategy<StorageValueType>,
                                           rl4::bpcm::JointCalculationStrategy<StorageValueType> >::with(&memRes).create();
        }

        template<typename TArray>
        void execute(const rl4::bpcm::Evaluator<StorageValueType>& joints, const TArray& expected, OutputScope scope) {
            auto outputInstance = joints.createInstance(&memRes);
            auto outputBuffer = outputInstance->getOutputBuffer();

            auto inputInstanceFactory =
                ControlsFactory::getInstanceFactory(0, static_cast<std::uint16_t>(block4::input::values.size()), 0, 0);
            auto inputInstance = inputInstanceFactory(&memRes);
            auto inputBuffer = inputInstance->getInputBuffer();
            std::copy(block4::input::values.begin(), block4::input::values.end(), inputBuffer.begin());

            rl4::ConstArrayView<float> expectedView{expected[scope.lod].data() + scope.offset, scope.size};
            rl4::ConstArrayView<float> outputView{outputBuffer.data() + scope.offset, scope.size};
            joints.calculate(inputInstance.get(), outputInstance.get(), scope.lod);
            ASSERT_EQ(outputView, expectedView);
        }

    protected:
        pma::AlignedMemoryResource memRes;
        block4::OptimizedStorage<StorageValueType>::StrategyPtr strategy;
};

}  // namespace

TEST_P(SSEJointCalculationStrategyTest, Block4Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 0ul, 1ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 0u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, Block4Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 1ul, 4ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 1u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, Block8Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 5ul, 7ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 2u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, Block8Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 12ul, 8ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 3u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, Block12Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 20ul, 9ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 4u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, Block12Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 29ul, 12ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 5u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, Block16Padded) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 41ul, 14ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 6u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, Block16Exact) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 55ul, 16ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 7u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, MultipleBlocks) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 0ul, block4::output::valuesPerLOD.front().size()};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, InputRegionA) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 71ul, 2ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 8u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

TEST_P(SSEJointCalculationStrategyTest, InputRegionB) {
    const auto params = GetParam();
    const OutputScope scope{params.lod, 73ul, 2ul};
    auto joints = block4::OptimizedStorage<StorageValueType>::create(std::move(strategy), 9u, &memRes);
    execute(joints, block4::output::valuesPerLOD, scope);
}

INSTANTIATE_TEST_SUITE_P(SSEJointCalculationStrategyTest, SSEJointCalculationStrategyTest, ::testing::Values(
                             StrategyTestParams{0u},
                             StrategyTestParams{1u},
                             StrategyTestParams{2u},
                             StrategyTestParams{3u}
                             ));

#endif  // RL_BUILD_WITH_SSE
