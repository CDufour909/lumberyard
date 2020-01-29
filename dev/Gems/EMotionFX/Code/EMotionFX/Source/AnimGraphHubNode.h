/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <EMotionFX/Source/EMotionFXConfig.h>
#include <EMotionFX/Source/AnimGraphNode.h>


namespace EMotionFX
{

    class EMFX_API AnimGraphHubNode
        : public AnimGraphNode
    {
    public:
        AZ_RTTI(AnimGraphHubNode, "{61771820-2619-462B-8114-75B8B701795D}", AnimGraphNode)
        AZ_CLASS_ALLOCATOR_DECL

        enum
        {
            OUTPUTPORT_RESULT   = 0
        };

        enum
        {
            PORTID_OUTPUT_POSE = 0
        };

        class EMFX_API UniqueData
            : public AnimGraphNodeData
        {
            EMFX_ANIMGRAPHOBJECTDATA_IMPLEMENT_LOADSAVE
        public:
            AZ_CLASS_ALLOCATOR_DECL

            UniqueData(AnimGraphNode* node, AnimGraphInstance* animGraphInstance)
                : AnimGraphNodeData(node, animGraphInstance)
                , m_sourceNode(nullptr) {}
            ~UniqueData() {}

        public:
            AnimGraphNode*  m_sourceNode;
        };


        AnimGraphHubNode();
        ~AnimGraphHubNode();

        bool InitAfterLoading(AnimGraph* animGraph) override;

        AZ::Color GetVisualColor() const override                   { return AZ::Color(0.2f, 0.78f, 0.59f, 1.0f); }
        bool GetCanActAsState() const override                      { return true; }
        bool GetSupportsVisualization() const override              { return true; }
        AnimGraphPose* GetMainOutputPose(AnimGraphInstance* animGraphInstance) const override     { return GetOutputPose(animGraphInstance, OUTPUTPORT_RESULT)->GetValue(); }
        bool GetHasOutputPose() const override                      { return true; }
        bool GetCanBeInsideStateMachineOnly() const override        { return true; }
        bool GetHasVisualOutputPorts() const override               { return false; }
        bool GetCanHaveOnlyOneInsideParent() const override         { return false; }
        void OnStateEntering(AnimGraphInstance* animGraphInstance, AnimGraphNode* previousState, AnimGraphStateTransition* usedTransition) override;
        void Rewind(AnimGraphInstance* animGraphInstance) override;
        AnimGraphNode* GetSourceNode(const AnimGraphInstance* animGraphInstance) const;
        const char* GetPaletteName() const override;
        AnimGraphObject::ECategory GetPaletteCategory() const override;

        void OnUpdateUniqueData(AnimGraphInstance* animGraphInstance) override;
        void UpdateUniqueData(AnimGraphInstance* animGraphInstance, UniqueData* uniqueData);

        static void Reflect(AZ::ReflectContext* context);

    private:
        void Output(AnimGraphInstance* animGraphInstance) override;
        void Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;
        void TopDownUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;
        void PostUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;
        void OnRemoveNode(AnimGraph* animGraph, AnimGraphNode* nodeToRemove) override;
    };
}   // namespace EMotionFX
