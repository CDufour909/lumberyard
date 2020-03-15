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

#include "BlendTreeBlendNNode.h"
#include "ActorInstance.h"
#include "AnimGraphAttributeTypes.h"
#include "AnimGraphInstance.h"
#include "AnimGraphManager.h"
#include "AnimGraphMotionNode.h"
#include "EMotionFXConfig.h"
#include "MotionInstance.h"
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <EMotionFX/Source/AnimGraphBus.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeBlendNNode, AnimGraphAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeBlendNNode::UniqueData, AnimGraphObjectUniqueDataAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendNParamWeight, AnimGraphAllocator, 0)

    BlendNParamWeight::BlendNParamWeight(AZ::u32 portId, float weightRange)
        : m_portId(portId)
        , m_weightRange(weightRange)
    {
    }

    const char* BlendNParamWeight::GetPortLabel() const
    {
        return BlendTreeBlendNNode::GetPoseInputPortName(m_portId);
    }

    AZ::u32 BlendNParamWeight::GetPortId() const
    {
        return m_portId;
    }

    float BlendNParamWeight::GetWeightRange() const
    {
        return m_weightRange;
    }

    void BlendNParamWeight::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendNParamWeight>()
            ->Version(1)
            ->Field("portId", &BlendNParamWeight::m_portId)
            ->Field("weightRange", &BlendNParamWeight::m_weightRange);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendNParamWeight>("Blend N Param Weight", "Blend N Param Weight")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->ElementAttribute(AZ::Edit::Attributes::NameLabelOverride, &BlendNParamWeight::GetPortLabel);
    }

    BlendTreeBlendNNode::BlendTreeBlendNNode()
        : AnimGraphNode()
        , m_syncMode(SYNCMODE_DISABLED)
        , m_eventMode(EVENTMODE_MOSTACTIVE)
    {
        // setup input ports
        InitInputPorts(11);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_0), INPUTPORT_POSE_0, AttributePose::TYPE_ID, PORTID_INPUT_POSE_0);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_1), INPUTPORT_POSE_1, AttributePose::TYPE_ID, PORTID_INPUT_POSE_1);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_2), INPUTPORT_POSE_2, AttributePose::TYPE_ID, PORTID_INPUT_POSE_2);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_3), INPUTPORT_POSE_3, AttributePose::TYPE_ID, PORTID_INPUT_POSE_3);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_4), INPUTPORT_POSE_4, AttributePose::TYPE_ID, PORTID_INPUT_POSE_4);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_5), INPUTPORT_POSE_5, AttributePose::TYPE_ID, PORTID_INPUT_POSE_5);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_6), INPUTPORT_POSE_6, AttributePose::TYPE_ID, PORTID_INPUT_POSE_6);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_7), INPUTPORT_POSE_7, AttributePose::TYPE_ID, PORTID_INPUT_POSE_7);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_8), INPUTPORT_POSE_8, AttributePose::TYPE_ID, PORTID_INPUT_POSE_8);
        SetupInputPort(GetPoseInputPortName(PORTID_INPUT_POSE_9), INPUTPORT_POSE_9, AttributePose::TYPE_ID, PORTID_INPUT_POSE_9);
        SetupInputPortAsNumber("Weight", INPUTPORT_WEIGHT, PORTID_INPUT_WEIGHT); // accept float/int/bool values

        // setup output ports
        InitOutputPorts(1);
        SetupOutputPortAsPose("Output Pose", OUTPUTPORT_POSE, PORTID_OUTPUT_POSE);
    }

    bool BlendTreeBlendNNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();
        Reinit();
        return true;
    }

    const char* BlendTreeBlendNNode::GetPaletteName() const
    {
        return "Blend N";
    }

    AnimGraphObject::ECategory BlendTreeBlendNNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_BLENDING;
    }

    // pre-create the unique data
    void BlendTreeBlendNNode::OnUpdateUniqueData(AnimGraphInstance* animGraphInstance)
    {
        // locate the existing unique data for this node
        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindUniqueObjectData(this));
        if (uniqueData == nullptr)
        {
            uniqueData = aznew UniqueData(this, animGraphInstance, MCORE_INVALIDINDEX32, MCORE_INVALIDINDEX32);
            animGraphInstance->RegisterUniqueObjectData(uniqueData);
        }

        // Initialize default connection custom weights
        // If this node has connections but no custom weights, it needs to set the default custom weight ranges
        if (m_paramWeights.empty())
        {
            float weightRange = 0.0f;
            const float defaultWeightStep = 1.0f;
            for (const AnimGraphNode::Port& port : mInputPorts)
            {
                if (port.mConnection && port.mPortID != PORTID_INPUT_WEIGHT)
                {
                    m_paramWeights.emplace_back(port.mPortID, weightRange);
                    weightRange += defaultWeightStep;
                }
            }
            if (!m_paramWeights.empty())
            {
                const float maxWeightRange = m_paramWeights.back().m_weightRange;
                if (maxWeightRange > AZ::g_fltEps)
                {
                    for (BlendNParamWeight& paramWeight : m_paramWeights)
                    {
                        paramWeight.m_weightRange /= maxWeightRange;
                    }
                }
            }
        }
    }

    // find the two blend nodes
    void BlendTreeBlendNNode::FindBlendNodes(AnimGraphInstance* animGraphInstance, AnimGraphNode** outNodeA, AnimGraphNode** outNodeB, uint32* outIndexA, uint32* outIndexB, float* outWeight) const
    {
        if (m_paramWeights.empty())
        {
            *outNodeA = nullptr;
            *outNodeB = nullptr;
            *outIndexA = MCORE_INVALIDINDEX32;
            *outIndexB = MCORE_INVALIDINDEX32;
            *outWeight = 0.0f;
            return;
        }

        float weight = m_paramWeights.front().m_weightRange;
        if (mDisabled == false)
        {
            if (mInputPorts[INPUTPORT_WEIGHT].mConnection)
            {
                weight = GetInputNumberAsFloat(animGraphInstance, INPUTPORT_WEIGHT);
            }
        }

        uint32 poseIndexA = m_paramWeights.front().m_portId;
        uint32 poseIndexB = m_paramWeights.front().m_portId;

        // If weight is <= minimum weight range the port id is the first for both poses (A and B) then the output weight is zero
        if (weight <= m_paramWeights.front().m_weightRange)
        {
            poseIndexA = m_paramWeights.front().m_portId;
            poseIndexB = poseIndexA;
            *outWeight = 0.0f;

            // Calculate the blend weight and get the nodes
            *outNodeA = GetInputPort(INPUTPORT_POSE_0 + poseIndexA).mConnection->GetSourceNode();
            *outNodeB = GetInputPort(INPUTPORT_POSE_0 + poseIndexB).mConnection->GetSourceNode();
            *outIndexA = poseIndexA;
            *outIndexB = poseIndexB;

            return;
        }

        // Searching for the index corresponding to the weight range in the sorted weight range array
        const size_t paramWeightCount = m_paramWeights.size();
        for (size_t i = 1; i < paramWeightCount; ++i)
        {
            if (weight <= m_paramWeights[i].m_weightRange)
            {
                poseIndexB = m_paramWeights[i].m_portId;
                poseIndexA = m_paramWeights[i - 1].m_portId;
                *outWeight = (weight - m_paramWeights[i - 1].m_weightRange)
                    / (m_paramWeights[i].m_weightRange - m_paramWeights[i - 1].m_weightRange);

                if (i == 1 && (*outWeight < MCore::Math::epsilon))
                {
                    // Snap to the minimum if for the first range check the value is near 0
                    poseIndexA = m_paramWeights.front().m_portId;
                    poseIndexB = poseIndexA;
                    *outWeight = 0.0f;
                }
                else if (i == (paramWeightCount - 1) && (*outWeight > 1.0f - MCore::Math::epsilon))
                {
                    // Snap to the maximum if for the first range check the value is near 1
                    poseIndexA = m_paramWeights.back().m_portId;
                    poseIndexB = poseIndexA;
                    *outWeight = 0.0f;
                }

                // Search complete: the input weight is between m_paramWeights[i] and m_paramWeights[i - 1]
                // Calculate the blend weight and get the nodes and then return
                *outNodeA = GetInputPort(INPUTPORT_POSE_0 + poseIndexA).mConnection->GetSourceNode();
                *outNodeB = GetInputPort(INPUTPORT_POSE_0 + poseIndexB).mConnection->GetSourceNode();
                *outIndexA = poseIndexA;
                *outIndexB = poseIndexB;

                return;
            }
        }

        // Not found in the range (nor below it) so snap to the maximum
        poseIndexA = m_paramWeights.back().m_portId;
        poseIndexB = poseIndexA;
        *outWeight = 0.0f;

        // Calculate the blend weight and get the nodes
        *outNodeA = GetInputPort(INPUTPORT_POSE_0 + poseIndexA).mConnection->GetSourceNode();
        *outNodeB = GetInputPort(INPUTPORT_POSE_0 + poseIndexB).mConnection->GetSourceNode();
        *outIndexA = poseIndexA;
        *outIndexB = poseIndexB;
    }

    void BlendTreeBlendNNode::SyncMotions(AnimGraphInstance* animGraphInstance, AnimGraphNode* nodeA, AnimGraphNode* nodeB, uint32 poseIndexA, uint32 poseIndexB, float blendWeight, ESyncMode syncMode)
    {
        if (nodeA == nullptr || nodeB == nullptr)
        {
            return;
        }

        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindUniqueNodeData(this));

        // check if we need to resync, this indicates the two motions we blend between changed
        bool resync = false;
        if (uniqueData->mIndexA != poseIndexA || uniqueData->mIndexB != poseIndexB)
        {
            resync = true;
            nodeA->RecursiveSetUniqueDataFlag(animGraphInstance, AnimGraphInstance::OBJECTFLAGS_RESYNC, true);
        }

        // Sync the primary node to this blend N node.
        nodeA->AutoSync(animGraphInstance, this, 0.0f, SYNCMODE_TRACKBASED, resync);

        // for all input ports (10 motion input poses)
        for (uint32 i = 0; i < 10; ++i)
        {
            // check if this port is used
            BlendTreeConnection* connection = mInputPorts[i].mConnection;
            if (connection == nullptr)
            {
                continue;
            }

            // mark this node recursively as synced
            if (animGraphInstance->GetIsObjectFlagEnabled(mObjectIndex, AnimGraphInstance::OBJECTFLAGS_SYNCED) == false)
            {
                connection->GetSourceNode()->RecursiveSetUniqueDataFlag(animGraphInstance, AnimGraphInstance::OBJECTFLAGS_SYNCED, true);
            }

            if (connection->GetSourceNode() == nodeA)
            {
                continue;
            }

            // get the node to sync, and check the resync flag
            AnimGraphNode* nodeToSync = connection->GetSourceNode();
            if (resync)
            {
                nodeToSync->RecursiveSetUniqueDataFlag(animGraphInstance, AnimGraphInstance::OBJECTFLAGS_RESYNC, true);
            }

            // perform the syncing
            nodeToSync->AutoSync(animGraphInstance, nodeA, blendWeight, syncMode, resync);
        }

        uniqueData->mIndexA = poseIndexA;
        uniqueData->mIndexB = poseIndexB;
    }

    // perform the calculations / actions
    void BlendTreeBlendNNode::Output(AnimGraphInstance* animGraphInstance)
    {
        ActorInstance* actorInstance = animGraphInstance->GetActorInstance();

        // get the output pose
        AnimGraphPose* outputPose;

        // if there are no connections, there is nothing to do
        if (mConnections.empty() || mDisabled)
        {
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();

            // output a bind pose
            outputPose->InitFromBindPose(actorInstance);

            // visualize it
            if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
            {
                actorInstance->DrawSkeleton(outputPose->GetPose(), mVisualizeColor);
            }
            return;
        }

        // output the input weight node
        BlendTreeConnection* connection = mInputPorts[INPUTPORT_WEIGHT].mConnection;
        if (connection)
        {
            OutputIncomingNode(animGraphInstance, connection->GetSourceNode());
        }

        // get two nodes that we receive input poses from, and get the blend weight
        float blendWeight;
        AnimGraphNode* nodeA;
        AnimGraphNode* nodeB;
        uint32 poseIndexA;
        uint32 poseIndexB;
        FindBlendNodes(animGraphInstance, &nodeA, &nodeB, &poseIndexA, &poseIndexB, &blendWeight);

        // if there are no input poses, there is nothing else to do
        if (nodeA == nullptr)
        {
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            outputPose->InitFromBindPose(actorInstance);

            // visualize it
            if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
            {
                actorInstance->DrawSkeleton(outputPose->GetPose(), mVisualizeColor);
            }
            return;
        }

        // if both nodes are equal we can just output the given pose
        OutputIncomingNode(animGraphInstance, nodeA);
        const AnimGraphPose* poseA = GetInputPose(animGraphInstance, INPUTPORT_POSE_0 + poseIndexA)->GetValue();
        if (nodeA == nodeB || blendWeight < MCore::Math::epsilon || nodeB == nullptr)
        {
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();

            *outputPose = *poseA;
            if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
            {
                actorInstance->DrawSkeleton(outputPose->GetPose(), mVisualizeColor);
            }
            return;
        }

        // get the second pose, and check if blending is still needed
        OutputIncomingNode(animGraphInstance, nodeB);
        const AnimGraphPose* poseB = GetInputPose(animGraphInstance, INPUTPORT_POSE_0 + poseIndexB)->GetValue();
        if (blendWeight > 1.0f - MCore::Math::epsilon)
        {
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            *outputPose = *poseB;

            // visualize it
            if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
            {
                actorInstance->DrawSkeleton(outputPose->GetPose(), mVisualizeColor);
            }
            return;
        }

        // perform the blend
        RequestPoses(animGraphInstance);
        outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
        *outputPose = *poseA;
        outputPose->GetPose().Blend(&poseB->GetPose(), blendWeight);

        // visualize it
        if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
        {
            actorInstance->DrawSkeleton(outputPose->GetPose(), mVisualizeColor);
        }
    }

    void BlendTreeBlendNNode::Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        // if the node is disabled
        if (mDisabled)
        {
            UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
            uniqueData->Clear();
            return;
        }

        // get the input weight
        BlendTreeConnection* connection = mInputPorts[INPUTPORT_WEIGHT].mConnection;
        if (connection)
        {
            UpdateIncomingNode(animGraphInstance, connection->GetSourceNode(), timePassedInSeconds);
        }

        // get two nodes that we receive input poses from, and get the blend weight
        float blendWeight;
        AnimGraphNode* nodeA;
        AnimGraphNode* nodeB;
        uint32 poseIndexA;
        uint32 poseIndexB;
        FindBlendNodes(animGraphInstance, &nodeA, &nodeB, &poseIndexA, &poseIndexB, &blendWeight);

        // if we have no input nodes
        if (nodeA == nullptr)
        {
            UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
            uniqueData->Clear();
            return;
        }

        UpdateIncomingNode(animGraphInstance, nodeA, timePassedInSeconds);
        if (nodeB && nodeA != nodeB)
        {
            UpdateIncomingNode(animGraphInstance, nodeB, timePassedInSeconds);
        }

        // update the sync track
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        uniqueData->Init(animGraphInstance, nodeA);

        // output the correct play speed
        float factorA;
        float factorB;
        float playSpeed;
        AnimGraphNode::CalcSyncFactors(animGraphInstance, nodeA, nodeB, m_syncMode, blendWeight, &factorA, &factorB, &playSpeed);
        uniqueData->SetPlaySpeed(playSpeed * factorA);
    }

    void BlendTreeBlendNNode::TopDownUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        // if the node is disabled
        if (mDisabled)
        {
            return;
        }

        // top down update the weight input
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        const BlendTreeConnection* con = GetInputPort(INPUTPORT_WEIGHT).mConnection;
        if (con)
        {
            con->GetSourceNode()->FindUniqueNodeData(animGraphInstance)->SetGlobalWeight(uniqueData->GetGlobalWeight());
            con->GetSourceNode()->PerformTopDownUpdate(animGraphInstance, timePassedInSeconds);
        }

        // get two nodes that we receive input poses from, and get the blend weight
        float blendWeight;
        AnimGraphNode* nodeA = nullptr;
        AnimGraphNode* nodeB = nullptr;
        uint32 poseIndexA;
        uint32 poseIndexB;
        FindBlendNodes(animGraphInstance, &nodeA, &nodeB, &poseIndexA, &poseIndexB, &blendWeight);

        // check if we want to sync the motions
        if (nodeA)
        {
            if (m_syncMode != SYNCMODE_DISABLED)
            {
                SyncMotions(animGraphInstance, nodeA, nodeB, poseIndexA, poseIndexB, blendWeight, m_syncMode);
            }
            else
            {
                nodeA->SetPlaySpeed(animGraphInstance, uniqueData->GetPlaySpeed() /* * nodeA->GetInternalPlaySpeed(animGraphInstance)*/);
                if (animGraphInstance->GetIsObjectFlagEnabled(nodeA->GetObjectIndex(), AnimGraphInstance::OBJECTFLAGS_SYNCED))
                {
                    nodeA->RecursiveSetUniqueDataFlag(animGraphInstance, AnimGraphInstance::OBJECTFLAGS_SYNCED, false);
                }
            }

            nodeA->FindUniqueNodeData(animGraphInstance)->SetGlobalWeight(uniqueData->GetGlobalWeight() * (1.0f - blendWeight));
            nodeA->FindUniqueNodeData(animGraphInstance)->SetLocalWeight(1.0f - blendWeight);
        }

        if (nodeB)
        {
            if (m_syncMode == SYNCMODE_DISABLED)
            {
                nodeB->SetPlaySpeed(animGraphInstance, uniqueData->GetPlaySpeed() /* * nodeB->GetInternalPlaySpeed(animGraphInstance)*/);
                if (animGraphInstance->GetIsObjectFlagEnabled(nodeB->GetObjectIndex(), AnimGraphInstance::OBJECTFLAGS_SYNCED))
                {
                    nodeB->RecursiveSetUniqueDataFlag(animGraphInstance, AnimGraphInstance::OBJECTFLAGS_SYNCED, false);
                }
            }

            nodeB->FindUniqueNodeData(animGraphInstance)->SetGlobalWeight(uniqueData->GetGlobalWeight() * blendWeight);
            nodeB->FindUniqueNodeData(animGraphInstance)->SetLocalWeight(blendWeight);
        }

        if (nodeA && nodeA == nodeB)
        {
            if (blendWeight < MCore::Math::epsilon)
            {
                nodeA->FindUniqueNodeData(animGraphInstance)->SetGlobalWeight(uniqueData->GetGlobalWeight());
                nodeA->FindUniqueNodeData(animGraphInstance)->SetLocalWeight(1.0f);
            }
            else
            {
                if (blendWeight > 1.0f - MCore::Math::epsilon)
                {
                    nodeA->FindUniqueNodeData(animGraphInstance)->SetGlobalWeight(0.0f);
                    nodeA->FindUniqueNodeData(animGraphInstance)->SetLocalWeight(0.0f);
                }
            }
        }

        // Top-down update the relevant nodes.
        nodeA->PerformTopDownUpdate(animGraphInstance, timePassedInSeconds);
        if (nodeB && nodeA != nodeB)
        {
            nodeB->PerformTopDownUpdate(animGraphInstance, timePassedInSeconds);
        }
    }

    // post sync update
    void BlendTreeBlendNNode::PostUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        // if we don't have enough inputs or are disabled, we don't need to update anything
        if (mDisabled)
        {
            // request the reference counted data inside the unique data
            RequestRefDatas(animGraphInstance);
            UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
            AnimGraphRefCountedData* data = uniqueData->GetRefCountedData();
            data->ClearEventBuffer();
            data->ZeroTrajectoryDelta();
            return;
        }

        // get the input weight
        BlendTreeConnection* connection = mInputPorts[INPUTPORT_WEIGHT].mConnection;
        if (connection)
        {
            connection->GetSourceNode()->PerformPostUpdate(animGraphInstance, timePassedInSeconds);
        }

        // get two nodes that we receive input poses from, and get the blend weight
        float blendWeight;
        AnimGraphNode* nodeA;
        AnimGraphNode* nodeB;
        uint32 poseIndexA;
        uint32 poseIndexB;
        FindBlendNodes(animGraphInstance, &nodeA, &nodeB, &poseIndexA, &poseIndexB, &blendWeight);

        // if we have no input nodes
        if (nodeA == nullptr)
        {
            // request the reference counted data inside the unique data
            RequestRefDatas(animGraphInstance);
            UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
            AnimGraphRefCountedData* data = uniqueData->GetRefCountedData();
            data->ClearEventBuffer();
            data->ZeroTrajectoryDelta();
            return;
        }

        nodeA->PerformPostUpdate(animGraphInstance, timePassedInSeconds);
        if (nodeB && nodeA != nodeB)
        {
            nodeB->PerformPostUpdate(animGraphInstance, timePassedInSeconds);
        }

        // request the reference counted data inside the unique data
        RequestRefDatas(animGraphInstance);
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        AnimGraphRefCountedData* data = uniqueData->GetRefCountedData();

        FilterEvents(animGraphInstance, m_eventMode, nodeA, nodeB, blendWeight, data);

        // if we have just one input node
        if (nodeA == nodeB || nodeB == nullptr)
        {
            AnimGraphRefCountedData* sourceData = nodeA->FindUniqueNodeData(animGraphInstance)->GetRefCountedData();
            data->SetTrajectoryDelta(sourceData->GetTrajectoryDelta());
            data->SetTrajectoryDeltaMirrored(sourceData->GetTrajectoryDeltaMirrored());
            return;
        }

        // extract motion from both
        Transform trajectoryDeltaA;
        Transform trajectoryDeltaB;
        Transform motionExtractDeltaA;
        Transform motionExtractDeltaB;

        AnimGraphRefCountedData* nodeAData = nodeA->FindUniqueNodeData(animGraphInstance)->GetRefCountedData();
        AnimGraphRefCountedData* nodeBData = nodeB->FindUniqueNodeData(animGraphInstance)->GetRefCountedData();

        // blend the results
        Transform delta = nodeAData->GetTrajectoryDelta();
        delta.Blend(nodeBData->GetTrajectoryDelta(), blendWeight);
        data->SetTrajectoryDelta(delta);

        // blend the mirrored results
        delta = nodeAData->GetTrajectoryDeltaMirrored();
        delta.Blend(nodeBData->GetTrajectoryDeltaMirrored(), blendWeight);
        data->SetTrajectoryDeltaMirrored(delta);
    }

    bool BlendTreeBlendNNode::VersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        const unsigned int version = classElement.GetVersion();
        if (version < 2)
        {
            AZStd::vector<BlendNParamWeight> paramWeights;
            classElement.AddElementWithData(context, "paramWeights", paramWeights);
        }
        return true;
    }

    void BlendTreeBlendNNode::Reflect(AZ::ReflectContext* context)
    {
        BlendNParamWeight::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeBlendNNode, AnimGraphNode>()
            ->Version(2, VersionConverter)
            ->Field("syncMode", &BlendTreeBlendNNode::m_syncMode)
            ->Field("eventMode", &BlendTreeBlendNNode::m_eventMode)
            ->Field("paramWeights", &BlendTreeBlendNNode::m_paramWeights);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendTreeBlendNNode>("Blend N", "Blend N attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &BlendTreeBlendNNode::m_syncMode)
            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &BlendTreeBlendNNode::m_eventMode)
            ->DataElement(AZ_CRC("BlendNParamWeightsContainerHandler", 0x311f6bb3), &BlendTreeBlendNNode::m_paramWeights, "Blend weight triggers", "The values of the input weight at which an input pose will weigh 100%")
            ->Attribute(AZ_CRC("BlendTreeBlendNNodeParamWeightsElement", 0x7eae1990), "")
            ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->ElementAttribute(AZ::Edit::UIHandlers::Handler, AZ_CRC("BlendNParamWeightsElementHandler", 0xec71620d));
    }

    void BlendTreeBlendNNode::SetSyncMode(ESyncMode syncMode)
    {
        m_syncMode = syncMode;
    }

    void BlendTreeBlendNNode::SetEventMode(EEventMode eventMode)
    {
        m_eventMode = eventMode;
    }

    void BlendTreeBlendNNode::SetParamWeightsEquallyDistributed(float min, float max)
    {
        if (m_paramWeights.empty())
        {
            return;
        }

        float weightStep = 0;
        if (m_paramWeights.size() > 1)
        {
            weightStep = (max - min) / (m_paramWeights.size() - 1);
        }
        m_paramWeights.back().m_weightRange = max;
        float weightRange = min;
        for (size_t i = 0; i < m_paramWeights.size() - 1; ++i)
        {
            m_paramWeights[i].m_weightRange = weightRange;
            weightRange += weightStep;
        }
    }

    void BlendTreeBlendNNode::UpdateParamWeights()
    {
        AZStd::unordered_map<AZ::u32, float> portToWeightRangeTable;
        for (const BlendNParamWeight& paramWeight : m_paramWeights)
        {
            portToWeightRangeTable.emplace(paramWeight.GetPortId(), paramWeight.GetWeightRange());
        }
        m_paramWeights.clear();

        const AZStd::vector<AnimGraphNode::Port>& inputPorts = GetInputPorts();
        int defaultElementsCount = 0;
        float* lastNonDefaultValue = nullptr;
        for (const AnimGraphNode::Port& port : inputPorts)
        {
            if (port.mConnection && port.mPortID != PORTID_INPUT_WEIGHT)
            {
                const float defaultRangeValue = m_paramWeights.empty() ? 0.0f : m_paramWeights.back().GetWeightRange();
                auto portToWeightRangeIterator = portToWeightRangeTable.find(port.mPortID);

                if (portToWeightRangeIterator == portToWeightRangeTable.end())
                {
                    // New connection just plugged
                    m_paramWeights.emplace_back(port.mPortID, defaultRangeValue);
                    defaultElementsCount++;
                }
                else
                {
                    // Existing connection, using existing weight range
                    m_paramWeights.emplace_back(port.mPortID, portToWeightRangeIterator->second);

                    // We want to fill the previous default values with uniformly distributed
                    // Weight ranges, if possible:
                    // Calculating the values to spread backwards to the previous default values
                    float weightRangeStep = 0;
                    if (lastNonDefaultValue)
                    {
                        weightRangeStep = (portToWeightRangeIterator->second - *lastNonDefaultValue) / (defaultElementsCount + 1);
                    }
                    float weightRange = portToWeightRangeIterator->second;
                    for (int i = 1; i <= defaultElementsCount; ++i)
                    {
                        weightRange -= weightRangeStep;
                        m_paramWeights[m_paramWeights.size() - 1 - i].m_weightRange = weightRange;
                    }
                    // Resetting the state of the default value calculator
                    defaultElementsCount = 0;
                    lastNonDefaultValue = &portToWeightRangeIterator->second;
                }
            }
        }

        AnimGraphNotificationBus::Broadcast(&AnimGraphNotificationBus::Events::OnSyncVisualObject, this);
    }

    const char* BlendTreeBlendNNode::GetPoseInputPortName(AZ::u32 portId)
    {
        const char* name = "";
        switch (portId)
        {
            case PORTID_INPUT_POSE_0:
                name = "Pose 0";
                break;
            case PORTID_INPUT_POSE_1:
                name = "Pose 1";
                break;
            case PORTID_INPUT_POSE_2:
                name = "Pose 2";
                break;
            case PORTID_INPUT_POSE_3:
                name = "Pose 3";
                break;
            case PORTID_INPUT_POSE_4:
                name = "Pose 4";
                break;
            case PORTID_INPUT_POSE_5:
                name = "Pose 5";
                break;
            case PORTID_INPUT_POSE_6:
                name = "Pose 6";
                break;
            case PORTID_INPUT_POSE_7:
                name = "Pose 7";
                break;
            case PORTID_INPUT_POSE_8:
                name = "Pose 8";
                break;
            case PORTID_INPUT_POSE_9:
                name = "Pose 9";
                break;
            default:
                AZ_Assert(false, "Error: unknown input port id %u", portId);
                break;
        }
        return name;
    }
} // namespace EMotionFX
