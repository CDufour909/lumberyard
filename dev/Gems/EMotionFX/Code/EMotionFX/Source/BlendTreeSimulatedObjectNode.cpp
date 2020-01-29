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

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/functional.h>
#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/Attachment.h>
#include <EMotionFX/Source/BlendTreeSimulatedObjectNode.h>
#include <EMotionFX/Source/EMotionFXManager.h>
#include <EMotionFX/Source/PhysicsSetup.h>
#include <EMotionFX/Source/Pose.h>
#include <EMotionFX/Source/SimulatedObjectSetup.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeSimulatedObjectNode, AnimGraphAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeSimulatedObjectNode::Simulation, AnimGraphAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeSimulatedObjectNode::UniqueData, AnimGraphObjectUniqueDataAllocator, 0)

    BlendTreeSimulatedObjectNode::UniqueData::~UniqueData()
    {
        for (Simulation* simulation : m_simulations)
        {
            delete simulation;
        }
    }

    BlendTreeSimulatedObjectNode::BlendTreeSimulatedObjectNode()
        : AnimGraphNode()
    {
        // Setup the input ports.
        InitInputPorts(5);
        SetupInputPort("Pose", INPUTPORT_POSE, AttributePose::TYPE_ID, PORTID_INPUT_POSE);
        SetupInputPortAsNumber("Stiffness factor", INPUTPORT_STIFFNESSFACTOR, PORTID_INPUT_STIFFNESSFACTOR);
        SetupInputPortAsNumber("Gravity factor", INPUTPORT_GRAVITYFACTOR, PORTID_INPUT_GRAVITYFACTOR);
        SetupInputPortAsNumber("Damping factor", INPUTPORT_DAMPINGFACTOR, PORTID_INPUT_DAMPINGFACTOR);
        SetupInputPortAsNumber("Weight", INPUTPORT_WEIGHT, PORTID_INPUT_WEIGHT);

        // Setup the output ports.
        InitOutputPorts(1);
        SetupOutputPortAsPose("Pose", OUTPUTPORT_POSE, PORTID_OUTPUT_POSE);
    }

    BlendTreeSimulatedObjectNode::~BlendTreeSimulatedObjectNode()
    {
        SimulatedObjectNotificationBus::Handler::BusDisconnect();
    }

    void BlendTreeSimulatedObjectNode::Reinit()
    {
        if (!mAnimGraph)
        {
            return;
        }

        SimulatedObjectNotificationBus::Handler::BusConnect();
        AnimGraphNode::Reinit();

        const size_t numInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);
            UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindUniqueNodeData(this));
            if (!uniqueData)
            {
                continue;
            }

            uniqueData->m_mustUpdate = true;
            animGraphInstance->UpdateUniqueData();
        }
    }

    bool BlendTreeSimulatedObjectNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();
        Reinit();
        return true;
    }

    const char* BlendTreeSimulatedObjectNode::GetPaletteName() const
    {
        return "Simulated Object";
    }

    AnimGraphObject::ECategory BlendTreeSimulatedObjectNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_PHYSICS;
    }

    float BlendTreeSimulatedObjectNode::GetStiffnessFactor(AnimGraphInstance* animGraphInstance) const
    {
        const MCore::AttributeFloat* input = GetInputFloat(animGraphInstance, INPUTPORT_STIFFNESSFACTOR);
        return input ? input->GetValue() : m_stiffnessFactor;
    }

    float BlendTreeSimulatedObjectNode::GetGravityFactor(AnimGraphInstance* animGraphInstance) const
    {
        const MCore::AttributeFloat* input = GetInputFloat(animGraphInstance, INPUTPORT_GRAVITYFACTOR);
        return input ? input->GetValue() : m_gravityFactor;
    }

    float BlendTreeSimulatedObjectNode::GetDampingFactor(AnimGraphInstance* animGraphInstance) const
    {
        const MCore::AttributeFloat* input = GetInputFloat(animGraphInstance, INPUTPORT_DAMPINGFACTOR);
        return input ? input->GetValue() : m_dampingFactor;
    }

    void BlendTreeSimulatedObjectNode::Rewind(AnimGraphInstance* animGraphInstance)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        for (Simulation* sim : uniqueData->m_simulations)
        {
            sim->m_solver.Stabilize();
        }
    }

    bool BlendTreeSimulatedObjectNode::InitSolvers(AnimGraphInstance* animGraphInstance, UniqueData* uniqueData)
    {
        // Delete existing solvers
        for (Simulation* sim : uniqueData->m_simulations)
        {
            delete sim;
        }
        uniqueData->m_simulations.clear();

        ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
        const Actor* actor = actorInstance->GetActor();
        const SimulatedObjectSetup* simObjectSetup = actor->GetSimulatedObjectSetup().get();
        if (!simObjectSetup)
        {
            //AZ_Warning("EMotionFX", false, "The actor has no simulated object setup, so the simulated object anim graph node '%s' cannot do anything.", GetName());
            return false;
        }

        // Give a warning for simulated object names that have been setup in this node, but that do not exist in the simulated object setup of this actor.
        for (const AZStd::string& simObjectName : m_simulatedObjectNames)
        {
            if (!simObjectSetup->FindSimulatedObjectByName(simObjectName.c_str()))
            {
                //AZ_Warning("EMotionFX", false, "Anim graph simulated object node '%s' references a simulated object with the name '%s', which does not exist in the simulated object setup for this actor.", GetName(), simObjectName.c_str());
            }
        }

        // Create and init a solver for each simulated object.
        auto& simulations = uniqueData->m_simulations;
        simulations.reserve(simObjectSetup->GetNumSimulatedObjects());
        for (const SimulatedObject* simObject : simObjectSetup->GetSimulatedObjects())
        {
            // Check if this simulated object is in our list of simulated objects that the user picked.
            // If not, then we can skip adding this simulated object.
            if (AZStd::find(m_simulatedObjectNames.begin(), m_simulatedObjectNames.end(), simObject->GetName()) == m_simulatedObjectNames.end())
            {
                continue;
            }

            // Create the simulation, which holds the solver.
            Simulation* sim = aznew Simulation();
            SpringSolver& solver = sim->m_solver;
            sim->m_simulatedObject = simObject;

            // Initialize the solver inside this sim object.
            SpringSolver::InitSettings initSettings;
            initSettings.m_actorInstance = actorInstance;
            initSettings.m_simulatedObject = simObject;
            initSettings.m_colliderTags = simObject->GetColliderTags();
            initSettings.m_name = GetName(); // The name is the anim graph node's name, used when printing some warning/error messages.
            if (!solver.Init(initSettings))
            {
                delete sim;
                continue;
            }
            solver.SetFixedTimeStep(1.0f / static_cast<float>(m_updateRate));
            solver.SetNumIterations(m_numIterations);
            solver.SetCollisionEnabled(m_collisionDetection);

            simulations.emplace_back(sim);
        }

        return true;
    }

    void BlendTreeSimulatedObjectNode::UpdateUniqueData(AnimGraphInstance* animGraphInstance, UniqueData* uniqueData)
    {
        if (uniqueData->m_mustUpdate)
        {
            uniqueData->m_mustUpdate = false;
            uniqueData->m_isValid = InitSolvers(animGraphInstance, uniqueData);
        }
    }

    void BlendTreeSimulatedObjectNode::Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        AnimGraphNode::Update(animGraphInstance, timePassedInSeconds);

        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        uniqueData->m_timePassedInSeconds = timePassedInSeconds;
    }

    void BlendTreeSimulatedObjectNode::Output(AnimGraphInstance* animGraphInstance)
    {
        AnimGraphPose* outputPose;

        // If nothing is connected to the input pose, output a bind pose.
        if (!GetInputPort(INPUTPORT_POSE).mConnection)
        {
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
            outputPose->InitFromBindPose(actorInstance);
            return;
        }

        // Get the weight from the input port.
        float weight = 1.0f;
        if (GetInputPort(INPUTPORT_WEIGHT).mConnection)
        {
            OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_WEIGHT));
            weight = GetInputNumberAsFloat(animGraphInstance, INPUTPORT_WEIGHT);
            weight = AZ::GetClamp(weight, 0.0f, 1.0f);
        }

        // If the weight is near zero or if this node is disabled, we can skip all calculations and just output the input pose.
        if (AZ::IsClose(weight, 0.0f, FLT_EPSILON) || mDisabled)
        {
            OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_POSE));
            const AnimGraphPose* inputPose = GetInputPose(animGraphInstance, INPUTPORT_POSE)->GetValue();
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            *outputPose = *inputPose;
            return;
        }

        OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_STIFFNESSFACTOR));
        OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_GRAVITYFACTOR));
        OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_DAMPINGFACTOR));

        // Get the input pose and copy it over to the output pose
        OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_POSE));
        AnimGraphPose* inputPose = GetInputPose(animGraphInstance, INPUTPORT_POSE)->GetValue();
        RequestPoses(animGraphInstance);
        outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
        *outputPose = *inputPose;

        // Check if we have a valid configuration.
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        UpdateUniqueData(animGraphInstance, uniqueData);
        if (!uniqueData->m_isValid)
        {
            if (GetEMotionFX().GetIsInEditorMode())
            {
                SetHasError(animGraphInstance, true);
            }
            return;
        }

        if (GetEMotionFX().GetIsInEditorMode())
        {
            SetHasError(animGraphInstance, false);
        }

        // If we are an attachment, update the transforms in the output pose.
        // It is possible that we are a skin attachment and we copy transforms from the main skeleton.
        Attachment* attachment = animGraphInstance->GetActorInstance()->GetSelfAttachment();
        if (attachment)
        {
            attachment->UpdateJointTransforms(outputPose->GetPose());
        }

        // Perform the solver update, and modify the output pose.
        for (Simulation* sim : uniqueData->m_simulations)
        {
            SpringSolver& solver = sim->m_solver;
            solver.SetStiffnessFactor(GetStiffnessFactor(animGraphInstance));
            solver.SetGravityFactor(GetGravityFactor(animGraphInstance));
            solver.SetDampingFactor(GetDampingFactor(animGraphInstance));
            solver.SetCollisionEnabled(m_collisionDetection);
            solver.Update(inputPose->GetPose(), outputPose->GetPose(), uniqueData->m_timePassedInSeconds, weight);
        }

        // Debug draw.
        if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
        {
            for (const Simulation* sim : uniqueData->m_simulations)
            {
                sim->m_solver.DebugRender(outputPose->GetPose(), m_collisionDetection, true, mVisualizeColor);
            }
        }
    }

    void BlendTreeSimulatedObjectNode::OnUpdateUniqueData(AnimGraphInstance* animGraphInstance)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindUniqueObjectData(this));
        if (!uniqueData)
        {
            uniqueData = aznew UniqueData(this, animGraphInstance);
            animGraphInstance->RegisterUniqueObjectData(uniqueData);
        }

        uniqueData->m_mustUpdate = true;
        UpdateUniqueData(animGraphInstance, uniqueData);
    }

    void BlendTreeSimulatedObjectNode::OnSimulatedObjectChanged()
    {
        const size_t numInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);
            OnUpdateUniqueData(animGraphInstance);
        }
    }

    void BlendTreeSimulatedObjectNode::SetSimulatedObjectNames(const AZStd::vector<AZStd::string>& simObjectNames)
    {
        m_simulatedObjectNames = simObjectNames;
    }

    void BlendTreeSimulatedObjectNode::AdjustParticles(const SpringSolver::ParticleAdjustFunction& func)
    {
        if (!mAnimGraph)
        {
            return;
        }

        const size_t numInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);
            UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindUniqueNodeData(this));
            if (!uniqueData)
            {
                continue;
            }

            for (Simulation* sim : uniqueData->m_simulations)
            {
                sim->m_solver.AdjustParticles(func);
            }
        }
    }

    void BlendTreeSimulatedObjectNode::OnPropertyChanged(const PropertyChangeFunction& func)
    {
        if (!mAnimGraph)
        {
            return;
        }

        const size_t numInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);
            UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindUniqueNodeData(this));
            if (!uniqueData)
            {
                continue;
            }

            func(uniqueData);
        }
    }

    void BlendTreeSimulatedObjectNode::OnNumIterationsChanged()
    {
        OnPropertyChanged([this](UniqueData* uniqueData) {
            for (Simulation* sim : uniqueData->m_simulations)
            {
                sim->m_solver.SetNumIterations(m_numIterations);
            }
        });
    }

    void BlendTreeSimulatedObjectNode::OnUpdateRateChanged()
    {
        OnPropertyChanged([this](UniqueData* uniqueData) {
            for (Simulation* sim : uniqueData->m_simulations)
            {
                sim->m_solver.SetFixedTimeStep(1.0f / static_cast<float>(m_updateRate));
            }
        });
    }

    void BlendTreeSimulatedObjectNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeSimulatedObjectNode, AnimGraphNode>()
            ->Version(1)
            ->Field("simulatedObjectNames", &BlendTreeSimulatedObjectNode::m_simulatedObjectNames)
            ->Field("stiffnessFactor", &BlendTreeSimulatedObjectNode::m_stiffnessFactor)
            ->Field("gravityFactor", &BlendTreeSimulatedObjectNode::m_gravityFactor)
            ->Field("dampingFactor", &BlendTreeSimulatedObjectNode::m_dampingFactor)
            ->Field("simulationRate", &BlendTreeSimulatedObjectNode::m_updateRate)
            ->Field("numIterations", &BlendTreeSimulatedObjectNode::m_numIterations)
            ->Field("collisionDetection", &BlendTreeSimulatedObjectNode::m_collisionDetection);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        const AZ::VectorFloat maxVecFloat = AZ::VectorFloat(std::numeric_limits<float>::max());
        editContext->Class<BlendTreeSimulatedObjectNode>("Simulated objects", "Simulated objects settings")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ_CRC("SimulatedObjectSelection", 0x846970e2), &BlendTreeSimulatedObjectNode::m_simulatedObjectNames, "Simulated object names", "The simulated objects we want to pick from this actor.")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeSimulatedObjectNode::Reinit)
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &BlendTreeSimulatedObjectNode::m_gravityFactor, "Gravity factor", "The gravity multiplier, which is a multiplier over the individual joint gravity values.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Max, 20.0f)
            ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &BlendTreeSimulatedObjectNode::m_stiffnessFactor, "Stiffness factor", "The stiffness multiplier, which is a multiplier over the individual joint stiffness values.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Max, 100.0f)
            ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &BlendTreeSimulatedObjectNode::m_dampingFactor, "Damping factor", "The damping multiplier, which is a multiplier over the individual joint damping values.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Max, 100.0f)
            ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &BlendTreeSimulatedObjectNode::m_updateRate, "Simulation update rate", "The simulation update rate, as number of frames per second.")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeSimulatedObjectNode::OnUpdateRateChanged)
            ->Attribute(AZ::Edit::Attributes::Min, 10)
            ->Attribute(AZ::Edit::Attributes::Max, 150)
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &BlendTreeSimulatedObjectNode::m_numIterations, "Number of iterations", "The number of iterations in the simulation. Higher values can be more stable. Lower numbers give faster performance.")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeSimulatedObjectNode::OnNumIterationsChanged)
            ->Attribute(AZ::Edit::Attributes::Min, 1)
            ->Attribute(AZ::Edit::Attributes::Max, 10)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeSimulatedObjectNode::m_collisionDetection, "Enable collisions", "Enable collision detection with its colliders?");
    }
} // namespace EMotionFX
