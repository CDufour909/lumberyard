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

#include <Editor/PropertyWidgets/ActorJointHandler.h>
#include <Editor/ActorEditorBus.h>
#include <EMotionFX/Tools/EMotionStudio/EMStudioSDK/Source/EMStudioManager.h>
#include <Editor/AnimGraphEditorBus.h>
#include <Editor/JointSelectionDialog.h>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>


namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(ActorJointPicker, EditorAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(ActorSingleJointHandler, EditorAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(ActorMultiJointHandler, EditorAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(ActorMultiWeightedJointHandler, EditorAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL_TEMPLATE(ActorJointElementHandler, EditorAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL_TEMPLATE(ActorWeightedJointElementHandler, EditorAllocator, 0)

    ActorJointPicker::ActorJointPicker(bool singleSelection, const QString& dialogTitle, const QString& dialogDescriptionLabelText, QWidget* parent)
        : QWidget(parent)
        , m_dialogTitle(dialogTitle)
        , m_dialogDescriptionLabelText(dialogDescriptionLabelText)
        , m_label(new QLabel())
        , m_pickButton(new QPushButton(QIcon(":/SceneUI/Manifest/TreeIcon.png"), ""))
        , m_resetButton(new QPushButton())
        , m_singleSelection(singleSelection)
    {
        connect(m_pickButton, &QPushButton::clicked, this, &ActorJointPicker::OnPickClicked);

        EMStudio::EMStudioManager::MakeTransparentButton(m_resetButton, "/Images/Icons/Clear.png", "Reset selection");
        connect(m_resetButton, &QPushButton::clicked, this, &ActorJointPicker::OnResetClicked);

        QHBoxLayout* hLayout = new QHBoxLayout();
        hLayout->setMargin(0);
        hLayout->addWidget(m_label);
        hLayout->addStretch();
        hLayout->addWidget(m_pickButton);
        hLayout->addWidget(m_resetButton);
        setLayout(hLayout);
    }

    void ActorJointPicker::AddDefaultFilter(const QString& category, const QString& displayName)
    {
        m_defaultFilters.emplace_back(category, displayName);
    }

    void ActorJointPicker::OnResetClicked()
    {
        SetWeightedJointNames(AZStd::vector<AZStd::pair<AZStd::string, float> >());
        emit SelectionChanged();
    }

    void ActorJointPicker::SetJointName(const AZStd::string& jointName)
    {
        AZStd::vector<AZStd::pair<AZStd::string, float> > weightedJointNames;

        if (!jointName.empty())
        {
            weightedJointNames.emplace_back(jointName, 0.0f);
        }

        SetWeightedJointNames(weightedJointNames);
    }

    AZStd::string ActorJointPicker::GetJointName() const
    {
        if (m_weightedJointNames.empty())
        {
            return AZStd::string();
        }

        return m_weightedJointNames[0].first;
    }

    void ActorJointPicker::SetJointNames(const AZStd::vector<AZStd::string>& jointNames)
    {
        AZStd::vector<AZStd::pair<AZStd::string, float> > weightedJointNames;

        const size_t numJointNames = jointNames.size();
        weightedJointNames.resize(numJointNames);

        for (size_t i = 0; i < numJointNames; ++i)
        {
            weightedJointNames[i] = AZStd::make_pair<AZStd::string, float>(jointNames[i], 0.0f);
        }

        SetWeightedJointNames(weightedJointNames);
    }

    AZStd::vector<AZStd::string> ActorJointPicker::GetJointNames() const
    {
        AZStd::vector<AZStd::string> result;

        const size_t numJoints = m_weightedJointNames.size();
        result.resize(numJoints);

        for (size_t i = 0; i < numJoints; ++i)
        {
            result[i] = m_weightedJointNames[i].first;
        }

        return result;
    }

    void ActorJointPicker::UpdateInterface()
    {
        const size_t numJoints = m_weightedJointNames.size();
        m_label->setText(QString("%1 joint%2 selected").arg(QString::number(numJoints), numJoints == 1 ? QString() : "s"));
        m_resetButton->setVisible(numJoints > 0);

        // Build and set the tooltip containing all joints.
        QString tooltip;
        for (size_t i = 0; i < numJoints; ++i)
        {
            tooltip += m_weightedJointNames[i].first.c_str();
            if (i != numJoints - 1)
            {
                tooltip += "\n";
            }
        }
        m_label->setToolTip(tooltip);
    }

    void ActorJointPicker::SetWeightedJointNames(const AZStd::vector<AZStd::pair<AZStd::string, float> >& weightedJointNames)
    {
        m_weightedJointNames = weightedJointNames;
        UpdateInterface();
    }

    AZStd::vector<AZStd::pair<AZStd::string, float> > ActorJointPicker::GetWeightedJointNames() const
    {
        return m_weightedJointNames;
    }

    void ActorJointPicker::OnPickClicked()
    {
        EMotionFX::ActorInstance* actorInstance = nullptr;
        ActorEditorRequestBus::BroadcastResult(actorInstance, &ActorEditorRequests::GetSelectedActorInstance);
        if (!actorInstance)
        {
            QMessageBox::warning(this, "No Actor Instance", "Cannot open joint selection window. No valid actor instance selected.");
            return;
        }

        // Create and show the joint picker window
        JointSelectionDialog jointSelectionWindow(m_singleSelection, m_dialogTitle, m_dialogDescriptionLabelText, this);

        for (const auto& filterPair : m_defaultFilters)
        {
            jointSelectionWindow.SetFilterState(filterPair.first, filterPair.second, true);
        }

        jointSelectionWindow.HideIcons();
        jointSelectionWindow.SelectByJointNames(GetJointNames());
        jointSelectionWindow.setModal(true);

        if (jointSelectionWindow.exec() != QDialog::Rejected)
        {
            SetJointNames(jointSelectionWindow.GetSelectedJointNames());
            emit SelectionChanged();
        }
    }

    //---------------------------------------------------------------------------------------------------------------------------------------------------------

    template<class T>
    AZ::u32 ActorJointElementHandlerImpl<T>::GetHandlerName() const
    {
        return AZ_CRC("ActorJointElement", 0xedc8946c);
    }

    template<>
    AZ::u32 ActorJointElementHandlerImpl<AZStd::pair<AZStd::string, float>>::GetHandlerName() const
    {
        return AZ_CRC("ActorWeightedJointElement", 0xe84566a0);
    }

    template<class T>
    QWidget* ActorJointElementHandlerImpl<T>::CreateGUI(QWidget* parent)
    {
        AZ_UNUSED(parent);
        return nullptr;
    }

    template<class T>
    void ActorJointElementHandlerImpl<T>::WriteGUIValuesIntoProperty(size_t index, QWidget* GUI, T& instance, AzToolsFramework::InstanceDataNode* node)
    {
    }

    template<class T>
    bool ActorJointElementHandlerImpl<T>::ReadValuesIntoGUI(size_t index, QWidget* GUI, const T& instance, AzToolsFramework::InstanceDataNode* node)
    {
        return true;
    }

    template class ActorJointElementHandlerImpl<AZStd::string>;
    template class ActorJointElementHandlerImpl<AZStd::pair<AZStd::string, float>>;

    //---------------------------------------------------------------------------------------------------------------------------------------------------------

    AZ::u32 ActorSingleJointHandler::GetHandlerName() const
    {
        return AZ_CRC("ActorNode", 0x35d9eb50);
    }

    QWidget* ActorSingleJointHandler::CreateGUI(QWidget* parent)
    {
        ActorJointPicker* picker = aznew ActorJointPicker(true /*singleSelection*/, "Select Joint", "Select or double-click a joint", parent);

        connect(picker, &ActorJointPicker::SelectionChanged, this, [picker]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, picker);
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::Bus::Handler::OnEditingFinished, picker);
        });

        return picker;
    }

    void ActorSingleJointHandler::ConsumeAttribute(ActorJointPicker* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == AZ::Edit::Attributes::ReadOnly)
        {
            bool value;
            if (attrValue->Read<bool>(value))
            {
                GUI->setEnabled(!value);
            }
        }
    }

    void ActorSingleJointHandler::WriteGUIValuesIntoProperty(size_t index, ActorJointPicker* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        instance = GUI->GetJointName();
    }

    bool ActorSingleJointHandler::ReadValuesIntoGUI(size_t index, ActorJointPicker* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);
        GUI->SetJointName(instance);
        return true;
    }

    //---------------------------------------------------------------------------------------------------------------------------------------------------------

    AZ::u32 ActorMultiJointHandler::GetHandlerName() const
    {
        return AZ_CRC("ActorNodes", 0x70504714);
    }

    QWidget* ActorMultiJointHandler::CreateGUI(QWidget* parent)
    {
        ActorJointPicker* picker = aznew ActorJointPicker(false /*singleSelection*/, "Select Joints", "Select one or more joints from the skeleton", parent);

        connect(picker, &ActorJointPicker::SelectionChanged, this, [picker]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, picker);
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::Bus::Handler::OnEditingFinished, picker);
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestRefresh, AzToolsFramework::Refresh_EntireTree);
        });

        return picker;
    }

    void ActorMultiJointHandler::ConsumeAttribute(ActorJointPicker* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == AZ::Edit::Attributes::ReadOnly)
        {
            bool value;
            if (attrValue->Read<bool>(value))
            {
                GUI->setEnabled(!value);
            }
        }
    }

    void ActorMultiJointHandler::WriteGUIValuesIntoProperty(size_t index, ActorJointPicker* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        instance = GUI->GetJointNames();
    }

    bool ActorMultiJointHandler::ReadValuesIntoGUI(size_t index, ActorJointPicker* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);
        GUI->SetJointNames(instance);
        return true;
    }

    //---------------------------------------------------------------------------------------------------------------------------------------------------------

    AZ::u32 ActorMultiWeightedJointHandler::GetHandlerName() const
    {
        return AZ_CRC("ActorWeightedNodes", 0x689c0537);
    }

    QWidget* ActorMultiWeightedJointHandler::CreateGUI(QWidget* parent)
    {
        ActorJointPicker* picker = aznew ActorJointPicker(false /*singleSelection*/, "Joint Selection Dialog", "Select one or more joints from the skeleton", parent);

        connect(picker, &ActorJointPicker::SelectionChanged, this, [picker]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, picker);
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::Bus::Handler::OnEditingFinished, picker);
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestRefresh, AzToolsFramework::Refresh_EntireTree);
        });

        return picker;
    }

    void ActorMultiWeightedJointHandler::ConsumeAttribute(ActorJointPicker* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == AZ::Edit::Attributes::ReadOnly)
        {
            bool value;
            if (attrValue->Read<bool>(value))
            {
                GUI->setEnabled(!value);
            }
        }
    }

    void ActorMultiWeightedJointHandler::WriteGUIValuesIntoProperty(size_t index, ActorJointPicker* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        instance = GUI->GetWeightedJointNames();
    }

    bool ActorMultiWeightedJointHandler::ReadValuesIntoGUI(size_t index, ActorJointPicker* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);
        GUI->SetWeightedJointNames(instance);
        return true;
    }
} // namespace EMotionFX
