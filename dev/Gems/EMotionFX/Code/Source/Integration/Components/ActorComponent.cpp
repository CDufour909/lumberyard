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

#include "EMotionFX_precompiled.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Math/Transform.h>

#include <AzFramework/Physics/Ragdoll.h>
#include <AzFramework/Physics/RagdollPhysicsBus.h>
#include <AzFramework/Physics/World.h>

#include <LmbrCentral/Animation/AttachmentComponentBus.h>
#include <LmbrCentral/Rendering/MeshComponentBus.h>

#include <Integration/Components/ActorComponent.h>
#include <Integration/Rendering/RenderBackendManager.h>

#include <EMotionFX/Source/Transform.h>
#include <EMotionFX/Source/RagdollInstance.h>
#include <EMotionFX/Source/DebugDraw.h>


namespace EMotionFX
{
    namespace Integration
    {
        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::Configuration::Reflect(AZ::ReflectContext* context)
        {
            auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<Configuration>()
                    ->Version(3)
                    ->Field("ActorAsset", &Configuration::m_actorAsset)
                    ->Field("MaterialPerLOD", &Configuration::m_materialPerLOD)
                    ->Field("RenderSkeleton", &Configuration::m_renderSkeleton)
                    ->Field("RenderCharacter", &Configuration::m_renderCharacter)
                    ->Field("RenderBounds", &Configuration::m_renderBounds)
                    ->Field("AttachmentType", &Configuration::m_attachmentType)
                    ->Field("AttachmentTarget", &Configuration::m_attachmentTarget)
                    ->Field("SkinningMethod", &Configuration::m_skinningMethod)
                    ->Field("LODLevel", &Configuration::m_lodLevel)
                ;
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::Reflect(AZ::ReflectContext* context)
        {
            auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                Configuration::Reflect(context);

                serializeContext->Class<ActorComponent, AZ::Component>()
                    ->Version(1)
                    ->Field("Configuration", &ActorComponent::m_configuration)
                ;

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Enum<EMotionFX::Integration::Space>("Space", "The transformation space.")
                        ->Value("Local Space", Space::LocalSpace)
                        ->Value("Model Space", Space::ModelSpace)
                        ->Value("World Space", Space::WorldSpace);
                }
            }

            auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
            if (behaviorContext)
            {
                behaviorContext->EBus<ActorComponentRequestBus>("ActorComponentRequestBus")
                    ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::Preview)
                    ->Event("GetJointIndexByName", &ActorComponentRequestBus::Events::GetJointIndexByName)
                    ->Event("GetJointTransform", &ActorComponentRequestBus::Events::GetJointTransform)
                    ->Event("AttachToEntity", &ActorComponentRequestBus::Events::AttachToEntity)
                    ->Event("DetachFromEntity", &ActorComponentRequestBus::Events::DetachFromEntity)
                    ->Event("DebugDrawRoot", &ActorComponentRequestBus::Events::DebugDrawRoot)
                    ->Event("GetRenderCharacter", &ActorComponentRequestBus::Events::GetRenderCharacter)
                    ->Event("SetRenderCharacter", &ActorComponentRequestBus::Events::SetRenderCharacter)
                    ->VirtualProperty("RenderCharacter", "GetRenderCharacter", "SetRenderCharacter")
                ;

                behaviorContext->Class<ActorComponent>()->RequestBus("ActorComponentRequestBus");

                behaviorContext->EBus<ActorComponentNotificationBus>("ActorComponentNotificationBus")
                    ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::List)
                    ->Event("OnActorInstanceCreated", &ActorComponentNotificationBus::Events::OnActorInstanceCreated)
                    ->Event("OnActorInstanceDestroyed", &ActorComponentNotificationBus::Events::OnActorInstanceDestroyed)
                ;
            }
        }

        //////////////////////////////////////////////////////////////////////////
        ActorComponent::Configuration::Configuration()
            : m_attachmentType(AttachmentType::None)
            , m_renderSkeleton(false)
            , m_renderCharacter(true)
            , m_renderBounds(false)
            , m_skinningMethod(SkinningMethod::DualQuat)
            , m_lodLevel(0)
            , m_actorAsset(AZ::Data::AssetLoadBehavior::NoLoad)
        {
        }

        //////////////////////////////////////////////////////////////////////////
        ActorComponent::ActorComponent(const Configuration* configuration)
            : m_debugDrawRoot(false)
        {
            if (configuration)
            {
                m_configuration = *configuration;
            }
        }

        //////////////////////////////////////////////////////////////////////////
        ActorComponent::~ActorComponent()
        {
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::Activate()
        {
            m_actorInstance.reset();

            AZ::Data::AssetBus::Handler::BusDisconnect();

            auto& cfg = m_configuration;

            if (cfg.m_actorAsset.GetId().IsValid())
            {
                AZ::Data::AssetBus::Handler::BusConnect(cfg.m_actorAsset.GetId());
                cfg.m_actorAsset.QueueLoad();
            }

            AZ::TickBus::Handler::BusConnect();

            const AZ::EntityId entityId = GetEntityId();
            LmbrCentral::AttachmentComponentNotificationBus::Handler::BusConnect(entityId);
            AzFramework::CharacterPhysicsDataRequestBus::Handler::BusConnect(entityId);
            AzFramework::RagdollPhysicsNotificationBus::Handler::BusConnect(entityId);

            if (cfg.m_attachmentTarget.IsValid())
            {
                AttachToEntity(cfg.m_attachmentTarget, cfg.m_attachmentType);
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::Deactivate()
        {
            AzFramework::RagdollPhysicsNotificationBus::Handler::BusDisconnect();
            AzFramework::CharacterPhysicsDataRequestBus::Handler::BusDisconnect();
            Physics::WorldNotificationBus::Handler::BusDisconnect();
            ActorComponentRequestBus::Handler::BusDisconnect();
            AZ::TickBus::Handler::BusDisconnect();
            ActorComponentNotificationBus::Handler::BusDisconnect();
            LmbrCentral::AttachmentComponentNotificationBus::Handler::BusDisconnect();
            AZ::TransformNotificationBus::MultiHandler::BusDisconnect();
            AZ::Data::AssetBus::Handler::BusDisconnect();

            DestroyActor();
            m_configuration.m_actorAsset.Release();
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::AttachToEntity(AZ::EntityId targetEntityId, AttachmentType attachmentType)
        {
            if (targetEntityId.IsValid() && targetEntityId != GetEntityId())
            {
                ActorComponentNotificationBus::Handler::BusDisconnect();
                ActorComponentNotificationBus::Handler::BusConnect(targetEntityId);

                AZ::TransformNotificationBus::MultiHandler::BusConnect(targetEntityId);
                m_attachmentTargetEntityId = targetEntityId;

                // There's no guarantee that we will receive a on transform change call for the target entity because of the entity activate order.
                // Enforce a transform query on target to get the correct initial transform.
                AZ::Transform transform;
                AZ::TransformBus::EventResult(transform, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM); // default to using our own TM
                AZ::TransformBus::EventResult(transform, targetEntityId, &AZ::TransformBus::Events::GetWorldTM); // attempt to get target's TM
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetWorldTM, transform); // set our TM
            }
            else
            {
                DetachFromEntity();
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::DetachFromEntity()
        {
            if (m_attachmentTargetActor)
            {
                m_attachmentTargetActor->RemoveAttachment(m_actorInstance.get());
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetParent, AZ::EntityId());
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetLocalTM, AZ::Transform::CreateIdentity());

                AZ::TransformNotificationBus::MultiHandler::BusDisconnect(m_attachmentTargetEntityId);
                m_attachmentTargetEntityId.SetInvalid();
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::DebugDrawRoot(bool enable)
        {
            m_debugDrawRoot = enable;
        }

        //////////////////////////////////////////////////////////////////////////
        bool ActorComponent::GetRenderCharacter() const
        {
            return m_configuration.m_renderCharacter;
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::SetRenderCharacter(bool enable)
        {
            if (m_configuration.m_renderCharacter != enable)
            {
                m_configuration.m_renderCharacter = enable;

                if (m_renderActorInstance)
                {
                    m_renderActorInstance->SetIsVisible(m_configuration.m_renderCharacter);
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
        {
            OnAssetReady(asset);
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
        {
            m_configuration.m_actorAsset = asset;

            CheckActorCreation();
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::CheckActorCreation()
        {
            if (m_configuration.m_actorAsset.IsReady())
            {
                // Create actor instance.
                auto* actorAsset = m_configuration.m_actorAsset.GetAs<ActorAsset>();
                AZ_Error("EMotionFX", actorAsset, "Actor asset is not valid.");
                if (!actorAsset)
                {
                    return;
                }

                DestroyActor();

                m_actorInstance = actorAsset->CreateInstance(GetEntity());
                if (!m_actorInstance)
                {
                    AZ_Error("EMotionFX", actorAsset, "Failed to create actor instance.");
                    return;
                }

                ActorComponentRequestBus::Handler::BusConnect(GetEntityId());

                ActorComponentNotificationBus::Event(
                    GetEntityId(),
                    &ActorComponentNotificationBus::Events::OnActorInstanceCreated,
                    m_actorInstance.get());

                m_actorInstance->SetLODLevel(m_configuration.m_lodLevel);

                // Setup initial transform and listen for transform changes.
                AZ::Transform transform;
                AZ::TransformBus::EventResult(transform, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);
                OnTransformChanged(transform, transform);
                AZ::TransformNotificationBus::MultiHandler::BusConnect(GetEntityId());

                m_actorInstance->UpdateWorldTransform();
                m_actorInstance->UpdateBounds(0, ActorInstance::EBoundsType::BOUNDS_STATIC_BASED);

                RenderBackend* renderBackend = AZ::Interface<RenderBackendManager>::Get()->GetRenderBackend();
                m_renderActorInstance.reset(renderBackend->CreateActorInstance(GetEntityId(),
                        m_actorInstance,
                        m_configuration.m_actorAsset,
                        m_configuration.m_materialPerLOD,
                        m_configuration.m_skinningMethod,
                        transform));
                if (m_renderActorInstance)
                {
                    m_renderActorInstance->SetIsVisible(m_configuration.m_renderCharacter);
                }

                // Reattach all attachments
                for (AZ::EntityId& attachment : m_attachments)
                {
                    LmbrCentral::AttachmentComponentRequestBus::Event(attachment, &LmbrCentral::AttachmentComponentRequestBus::Events::Reattach, true);
                }

                LmbrCentral::AttachmentComponentRequestBus::Event(GetEntityId(), &LmbrCentral::AttachmentComponentRequestBus::Events::Reattach, true);

                CheckAttachToEntity();

                // Send general mesh creation notification to interested parties.
                LmbrCentral::MeshComponentNotificationBus::Event(GetEntityId(), &LmbrCentral::MeshComponentNotifications::OnMeshCreated, actorAsset);

                AzFramework::CharacterPhysicsDataNotificationBus::Event(GetEntityId(), &AzFramework::CharacterPhysicsDataNotifications::OnRagdollConfigurationReady);

                // Start listening to PostWorldUpdate events for the ragdoll.
                if (m_actorInstance->GetRagdollInstance())
                {
                    Physics::WorldNotificationBus::Handler::BusConnect(m_actorInstance->GetRagdollInstance()->GetRagdollWorldId());
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::CheckAttachToEntity()
        {
            // Attach to the target actor if we're both ready.
            // Note that m_attachmentTargetActor will always be null if we're not configured to attach to anything.
            if (m_actorInstance && m_attachmentTargetActor)
            {
                DetachFromEntity();

                // Make sure we don't generate some circular loop by attaching to each other.
                if (!m_attachmentTargetActor.get()->CheckIfCanHandleAttachment(m_actorInstance.get()))
                {
                    AZ_Error("EMotionFX", false, "You cannot attach to yourself or create circular dependencies!\n");
                    return;
                }

                // Create the attachment.
                AZ_Assert(m_configuration.m_attachmentType == AttachmentType::SkinAttachment, "Expected a skin attachment.");
                Attachment* attachment = AttachmentSkin::Create(m_attachmentTargetActor.get(), m_actorInstance.get());
                m_actorInstance->SetLocalSpaceTransform(Transform());
                m_attachmentTargetActor->AddAttachment(attachment);
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetParent, m_attachmentTargetActor->GetEntityId());
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetLocalTM, AZ::Transform::CreateIdentity());
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::DestroyActor()
        {
            m_renderActorInstance.reset();

            if (m_actorInstance)
            {
                DetachFromEntity();

                // Send general mesh destruction notification to interested parties.
                LmbrCentral::MeshComponentNotificationBus::Event(
                    GetEntityId(), 
                    &LmbrCentral::MeshComponentNotifications::OnMeshDestroyed);

                ActorComponentNotificationBus::Event(
                    GetEntityId(),
                    &ActorComponentNotificationBus::Events::OnActorInstanceDestroyed,
                    m_actorInstance.get());

                m_actorInstance.reset();
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world)
        {
            AZ_UNUSED(local);

            const AZ::EntityId* busIdPtr = AZ::TransformNotificationBus::GetCurrentBusId();
            if (!busIdPtr || *busIdPtr == GetEntityId()) // Our own entity has moved.
            {
                // If we're not attached to another actor, keep the EMFX root in sync with any external changes to the entity's transform.
                if (m_actorInstance)
                {
                    const Transform localTransform = m_actorInstance->GetParentWorldSpaceTransform().Inversed() * MCore::AzTransformToEmfxTransform(world);
                    m_actorInstance->SetLocalSpacePosition(localTransform.mPosition);
                    m_actorInstance->SetLocalSpaceRotation(localTransform.mRotation);

                    // Disable updating the scale to prevent feedback from adding up.
                    // We need to find a better way to handle this or to prevent this feedback loop.
                    //m_actorInstance->SetLocalSpaceScale(localTransform.mScale);
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
        {
            if (!m_actorInstance || !m_actorInstance->GetIsEnabled())
            {
                return;
            }

            if (m_renderActorInstance)
            {
                m_renderActorInstance->OnTick(deltaTime);
                m_renderActorInstance->UpdateBounds();

                RenderActorInstance::DebugOptions debugOptions;
                debugOptions.m_drawAABB = m_configuration.m_renderBounds;
                debugOptions.m_drawSkeleton = m_configuration.m_renderSkeleton;
                debugOptions.m_drawRootTransform = m_debugDrawRoot;
                debugOptions.m_rootWorldTransform = GetEntity()->GetTransform()->GetWorldTM();
                debugOptions.m_emfxDebugDraw = true;
                m_renderActorInstance->DebugDraw(debugOptions);
            }
        }

        int ActorComponent::GetTickOrder()
        {
            return AZ::TICK_PRE_RENDER;
        }

        void ActorComponent::OnPostPhysicsUpdate(float fixedDeltaTime)
        {
            if (m_actorInstance)
            {
                m_actorInstance->PostPhysicsUpdate(fixedDeltaTime);
            }
        }

        int ActorComponent::GetPhysicsTickOrder()
        {
            return WorldNotifications::Animation;
        }

        //////////////////////////////////////////////////////////////////////////
        void ActorComponent::OnActorInstanceCreated(ActorInstance* actorInstance)
        {
            auto it = AZStd::find(m_attachments.begin(), m_attachments.end(), actorInstance->GetEntityId());
            if (it != m_attachments.end())
            {
                if (m_actorInstance)
                {
                    LmbrCentral::AttachmentComponentRequestBus::Event(actorInstance->GetEntityId(), &LmbrCentral::AttachmentComponentRequestBus::Events::Reattach, true);
                }
            }
            else
            {
                m_attachmentTargetActor.reset(actorInstance);

                CheckAttachToEntity();
            }
        }

        void ActorComponent::OnActorInstanceDestroyed(ActorInstance* actorInstance)
        {
            DetachFromEntity();

            m_attachmentTargetActor = nullptr;
        }

        bool ActorComponent::GetRagdollConfiguration(Physics::RagdollConfiguration& ragdollConfiguration) const
        {
            if (!m_actorInstance)
            {
                return false;
            }

            const AZStd::shared_ptr<PhysicsSetup>& physicsSetup = m_actorInstance->GetActor()->GetPhysicsSetup();
            ragdollConfiguration = physicsSetup->GetRagdollConfig();
            return true;
        }

        AZStd::string ActorComponent::GetParentNodeName(const AZStd::string& childName) const
        {
            if (!m_actorInstance)
            {
                return AZStd::string();
            }

            const Skeleton* skeleton = m_actorInstance->GetActor()->GetSkeleton();
            Node* childNode = skeleton->FindNodeByName(childName);
            if (childNode)
            {
                const Node* parentNode = childNode->GetParentNode();
                if (parentNode)
                {
                    return parentNode->GetNameString();
                }
            }

            return AZStd::string();
        }

        //////////////////////////////////////////////////////////////////////////
        Physics::RagdollState ActorComponent::GetBindPose(const Physics::RagdollConfiguration& config) const
        {
            Physics::RagdollState physicsPose;

            if (!m_actorInstance)
            {
                return physicsPose;
            }

            const Actor* actor = m_actorInstance->GetActor();
            const Skeleton* skeleton = actor->GetSkeleton();
            const Pose* emfxPose = actor->GetBindPose();

            size_t numNodes = config.m_nodes.size();
            physicsPose.resize(numNodes);

            for (size_t nodeIndex = 0; nodeIndex < numNodes; nodeIndex++)
            {
                const char* nodeName = config.m_nodes[nodeIndex].m_debugName.data();
                Node* emfxNode = skeleton->FindNodeByName(nodeName);
                AZ_Error("EMotionFX", emfxNode, "Could not find bind pose for node %s", nodeName);
                if (emfxNode)
                {
                    const Transform& nodeTransform = emfxPose->GetModelSpaceTransform(emfxNode->GetNodeIndex());
                    physicsPose[nodeIndex].m_position = nodeTransform.mPosition;
                    physicsPose[nodeIndex].m_orientation = nodeTransform.mRotation;
                }
            }

            return physicsPose;
        }

        void ActorComponent::OnRagdollActivated()
        {
            Physics::Ragdoll* ragdoll;
            AzFramework::RagdollPhysicsRequestBus::EventResult(ragdoll, m_entity->GetId(), &AzFramework::RagdollPhysicsRequestBus::Events::GetRagdoll);
            if (ragdoll && m_actorInstance)
            {
                m_actorInstance->SetRagdoll(ragdoll);
            }
        }

        void ActorComponent::OnRagdollDeactivated()
        {
            if (m_actorInstance)
            {
                m_actorInstance->SetRagdoll(nullptr);
            }
        }

        size_t ActorComponent::GetJointIndexByName(const char* name) const
        {
            AZ_Assert(m_actorInstance, "The actor instance needs to be valid.");

            Node* node = m_actorInstance->GetActor()->GetSkeleton()->FindNodeByNameNoCase(name);
            if (node)
            {
                return static_cast<size_t>(node->GetNodeIndex());
            }

            return ActorComponentRequests::s_invalidJointIndex;
        }


        AZ::Transform ActorComponent::GetJointTransform(size_t jointIndex, Space space) const
        {
            AZ_Assert(m_actorInstance, "The actor instance needs to be valid.");

            const AZ::u32 index = static_cast<AZ::u32>(jointIndex);
            const AZ::u32 numNodes = m_actorInstance->GetActor()->GetNumNodes();

            AZ_Error("EMotionFX", index < numNodes, "GetJointTransform: The joint index %d is out of bounds [0;%d]. Entity: %s",
                index, numNodes, GetEntity()->GetName());

            if (index >= numNodes)
            {
                return AZ::Transform::CreateIdentity();
            }

            Pose* currentPose = m_actorInstance->GetTransformData()->GetCurrentPose();
            switch (space)
            {
            case Space::LocalSpace:
            {
                return MCore::EmfxTransformToAzTransform(currentPose->GetLocalSpaceTransform(index));
            }

            case Space::ModelSpace:
            {
                return MCore::EmfxTransformToAzTransform(currentPose->GetModelSpaceTransform(index));
            }

            case Space::WorldSpace:
            {
                return MCore::EmfxTransformToAzTransform(currentPose->GetWorldSpaceTransform(index));
            }

            default:
                AZ_Assert(false, "Unsupported space in GetJointTransform!");
            }

            return AZ::Transform::CreateIdentity();
        }

        void ActorComponent::GetJointTransformComponents(size_t jointIndex, Space space, AZ::Vector3& outPosition, AZ::Quaternion& outRotation, AZ::Vector3& outScale) const
        {
            AZ_Assert(m_actorInstance, "The actor instance needs to be valid.");

            const AZ::u32 index = static_cast<AZ::u32>(jointIndex);
            const AZ::u32 numNodes = m_actorInstance->GetActor()->GetNumNodes();

            AZ_Error("EMotionFX", index < numNodes, "GetJointTransformComponents: The joint index %d is out of bounds [0;%d]. Entity: %s",
                index, numNodes, GetEntity()->GetName());

            if (index >= numNodes)
            {
                return;
            }

            Pose* currentPose = m_actorInstance->GetTransformData()->GetCurrentPose();

            switch (space)
            {
            case Space::LocalSpace:
            {
                const Transform& localTransform = currentPose->GetLocalSpaceTransform(index);
                outPosition = localTransform.mPosition;
                outRotation = localTransform.mRotation;
                EMFX_SCALECODE
                (
                    outScale = localTransform.mScale;
                )
                return;
            }

            case Space::ModelSpace:
            {
                const Transform& modelTransform = currentPose->GetModelSpaceTransform(index);
                outPosition = modelTransform.mPosition;
                outRotation = modelTransform.mRotation;
                EMFX_SCALECODE
                (
                    outScale = modelTransform.mScale;
                )
                return;
            }

            case Space::WorldSpace:
            {
                const Transform worldTransform = currentPose->GetWorldSpaceTransform(index);
                outPosition = worldTransform.mPosition;
                outRotation = worldTransform.mRotation;
                EMFX_SCALECODE
                (
                    outScale = worldTransform.mScale;
                )
                return;
            }

            default:
            {
                AZ_Assert(false, "Unsupported space in GetJointTransform!");
                outPosition = AZ::Vector3::CreateZero();
                outRotation = AZ::Quaternion::CreateIdentity();
                outScale = AZ::Vector3::CreateOne();
            }
            }
        }

        Physics::AnimationConfiguration* ActorComponent::GetPhysicsConfig() const
        {
            if (m_actorInstance)
            {
                Actor* actor = m_actorInstance->GetActor();
                const AZStd::shared_ptr<PhysicsSetup>& physicsSetup = actor->GetPhysicsSetup();
                if (physicsSetup)
                {
                    return &physicsSetup->GetConfig();
                }
            }

            return nullptr;
        }

        // The entity has attached to the target.
        void ActorComponent::OnAttached(AZ::EntityId attachedEntityId)
        {
            const AZ::EntityId* busIdPtr = LmbrCentral::AttachmentComponentNotificationBus::GetCurrentBusId();
            if (busIdPtr)
            {
                const auto result = AZStd::find(m_attachments.begin(), m_attachments.end(), attachedEntityId);
                if (result == m_attachments.end())
                {
                    m_attachments.emplace_back(attachedEntityId);
                }
            }

            if (!m_actorInstance)
            {
                return;
            }

            ActorInstance* targetActorInstance = nullptr;
            ActorComponentRequestBus::EventResult(targetActorInstance, attachedEntityId, &ActorComponentRequestBus::Events::GetActorInstance);

            const char* jointName = nullptr;
            LmbrCentral::AttachmentComponentRequestBus::EventResult(jointName, attachedEntityId, &LmbrCentral::AttachmentComponentRequestBus::Events::GetJointName);
            if (targetActorInstance)
            {
                Node* node = jointName ? m_actorInstance->GetActor()->GetSkeleton()->FindNodeByName(jointName) : m_actorInstance->GetActor()->GetSkeleton()->GetNode(0);
                if (node)
                {
                    const AZ::u32 jointIndex = node->GetNodeIndex();
                    Attachment* attachment = AttachmentNode::Create(m_actorInstance.get(), jointIndex, targetActorInstance, true /* Managed externally, by this component. */);
                    m_actorInstance->AddAttachment(attachment);
                }
            }
        }


        // The entity is detaching from the target.
        void ActorComponent::OnDetached(AZ::EntityId targetId)
        {
            // Remove the targetId from the attachment list
            const AZ::EntityId* busIdPtr = LmbrCentral::AttachmentComponentNotificationBus::GetCurrentBusId();
            if (busIdPtr)
            {
                m_attachments.erase(AZStd::remove(m_attachments.begin(), m_attachments.end(), targetId), m_attachments.end());
            }

            if (!m_actorInstance)
            {
                return;
            }

            ActorInstance* targetActorInstance = nullptr;
            ActorComponentRequestBus::EventResult(targetActorInstance, targetId, &ActorComponentRequestBus::Events::GetActorInstance);
            if (targetActorInstance)
            {
                m_actorInstance->RemoveAttachment(targetActorInstance);
            }
        }
    } // namespace Integration
} // namespace EMotionFX
