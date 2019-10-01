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

#ifndef OUTLINER_VIEW_MODEL_H
#define OUTLINER_VIEW_MODEL_H

#include <AzCore/base.h>
#include <QtWidgets/QWidget>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QCheckBox>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/UI/SearchWidget/SearchCriteriaWidget.hxx>
#include "OutlinerSearchWidget.h"

#include <AzToolsFramework/ToolsComponents/EditorLockComponentBus.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/Entity/EditorEntityInfoBus.h>
#include <AzToolsFramework/API/EntityCompositionNotificationBus.h>

#pragma once

namespace EntityOutliner
{
    enum class DisplaySortMode : unsigned char;
}

//! Model for items in the OutlinerTreeView.
//! Each item represents an Entity.
//! Items are parented in the tree according to their transform hierarchy.
class OutlinerListModel
    : public QAbstractItemModel
    , private AzToolsFramework::EditorEntityContextNotificationBus::Handler
    , private AzToolsFramework::EditorEntityInfoNotificationBus::Handler
    , private AzToolsFramework::ToolsApplicationEvents::Bus::Handler
    , private AzToolsFramework::EntityCompositionNotificationBus::Handler
    , private AZ::EntitySystemBus::Handler
{
    Q_OBJECT;

public:
    AZ_CLASS_ALLOCATOR(OutlinerListModel, AZ::SystemAllocator, 0);

    //! Columns of data to display about each Entity.
    enum Column
    {
        ColumnName,                 //!< Entity name
        ColumnVisibilityToggle,     //!< Visibility Icons
        ColumnLockToggle,           //!< Lock Icons
        ColumnSortIndex,            //!< Index of sort order
        ColumnCount                 //!< Total number of columns
    };

    enum EntryType
    {
        EntityType,
        SliceEntityType,
        SliceHandleType,
        LayerType
    };

    enum Roles
    {
        VisibilityRole = Qt::UserRole + 1,
        SliceBackgroundRole,
        SliceEntityOverrideRole,
        EntityIdRole,
        EntityTypeRole,
        LayerColorRole,
        SelectedRole,
        ChildSelectedRole,
        PartiallyVisibleRole,
        PartiallyLockedRole,
        InLockedLayerRole,
        InInvisibleLayerRole,
        ChildCountRole,
        ExpandedRole,
        RoleCount
    };

    enum EntityIcon
    {
        SliceHandleIcon,            // Icon used to decorate slice handles
        BrokenSliceHandleIcon,      // Icon used to decorate broken slice handles
        SliceEntityIcon,            // Icon used to decorate entities that are part of a slice instantiation
        StandardEntityIcon          // Icon used to decorate entities that are not part of a slice instantiation
    };

    enum class GlobalSearchCriteriaFlags : int
    {
        Unlocked = 1 << static_cast<int>(AzQtComponents::OutlinerSearchWidget::GlobalSearchCriteria::Unlocked),
        Locked = 1 << static_cast<int>(AzQtComponents::OutlinerSearchWidget::GlobalSearchCriteria::Locked),
        Visible = 1 << static_cast<int>(AzQtComponents::OutlinerSearchWidget::GlobalSearchCriteria::Visible),
        Hidden = 1 << static_cast<int>(AzQtComponents::OutlinerSearchWidget::GlobalSearchCriteria::Hidden)
    };

    struct ComponentTypeValue
    {
        AZ::Uuid m_uuid;
        int m_globalVal;
    };

    // Spacing is appropriate and matches the outliner concept work from the UI team.
    static const int s_OutlinerSpacing = 5;

    static bool s_paintingName;

    OutlinerListModel(QObject* parent = nullptr);
    ~OutlinerListModel();

    void Initialize();

    // Qt overrides.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;

    bool IsSelected(const AZ::EntityId& entityId) const;

    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    QStringList mimeTypes() const override;

    QString GetSliceAssetName(const AZ::EntityId& entityId) const;

    QModelIndex GetIndexFromEntity(const AZ::EntityId& entityId, int column = 0) const;
    AZ::EntityId GetEntityFromIndex(const QModelIndex& index) const;

    bool FilterEntity(const AZ::EntityId& entityId);

    void EnableAutoExpand(bool enable);

    AZStd::string GetFilterString() const
    {
        return m_filterString;
    }

    static int GetLayerStripeWidth()
    {
        return 1;
    }

    void SetSortMode(EntityOutliner::DisplaySortMode sortMode) { m_sortMode = sortMode; }
    void SetDropOperationInProgress(bool inProgress);
Q_SIGNALS:
    void ExpandEntity(const AZ::EntityId& entityId, bool expand);
    void SelectEntity(const AZ::EntityId& entityId, bool select);
    void EnableSelectionUpdates(bool enable);
    void ResetFilter();
    void ReapplyFilter();

    public Q_SLOTS:
    void SearchStringChanged(const AZStd::string& filter);
    void SearchFilterChanged(const AZStd::vector<ComponentTypeValue>& componentFilters);
    void OnEntityExpanded(const AZ::EntityId& entityId);
    void OnEntityCollapsed(const AZ::EntityId& entityId);

    // Buffer Processing Slots - These are called using single-shot events when the buffers begin to fill.
    bool CanReparentEntities(const AZ::EntityId& newParentId, const AzToolsFramework::EntityIdList& selectedEntityIds) const;
    bool ReparentEntities(const AZ::EntityId& newParentId, const AzToolsFramework::EntityIdList& selectedEntityIds, const AZ::EntityId& beforeEntityId = AZ::EntityId());

    //! Use the current filter setting and re-evaluate the filter.
    void InvalidateFilter();

protected:

    //! Editor entity context notification bus
    void OnEditorEntitiesReplacedBySlicedEntities(const AZStd::unordered_map<AZ::EntityId, AZ::EntityId>& replacedEntitiesMap) override;
    void OnEditorEntityDuplicated(const AZ::EntityId& oldEntity, const AZ::EntityId& newEntity) override;
    void OnContextReset() override;

    //! Editor component lock interface to enable/disable selection of entity in the viewport.
    //! Setting the editor lock state on a parent will recursively set the flag on all descendants as well. (to match visibility)
    void ToggleEditorLockState(const AZ::EntityId& entityId);
    void SetEditorLockState(const AZ::EntityId& entityId, bool isLocked);
    void SetEditorLockStateRecursively(const AZ::EntityId& entityId, bool isLocked, const AZ::EntityId& toggledEntityId, bool toggledEntityWasLayer);

    //! Editor Visibility interface to enable/disable rendering in the viewport.
    //! Setting the editor visibility on a parent will recursively set the flag on all descendants as well.
    void ToggleEditorVisibility(const AZ::EntityId& entityId);
    void SetEditorVisibility(const AZ::EntityId& entityId, bool isVisible);
    void SetEditorVisibilityStateRecursively(const AZ::EntityId& entityId, bool isVisible, const AZ::EntityId& toggledEntityId, bool toggledEntityWasLayer);

    bool IsEntityVisible(const AZ::EntityId& entityId) const;
    void SetEntityVisibility(const AZ::EntityId& entityId, bool visibility) const;

    void QueueEntityUpdate(AZ::EntityId entityId);
    void QueueAncestorUpdate(AZ::EntityId entityId);
    void QueueEntityToExpand(AZ::EntityId entityId, bool expand);
    void ProcessEntityUpdates();
    void ProcessEntityInfoResetEnd();
    AZStd::unordered_set<AZ::EntityId> m_entitySelectQueue;
    AZStd::unordered_set<AZ::EntityId> m_entityExpandQueue;
    AZStd::unordered_set<AZ::EntityId> m_entityChangeQueue;
    bool m_entityChangeQueued;
    bool m_entityLayoutQueued;
    bool m_dropOperationInProgress = false;

    bool m_autoExpandEnabled = true;
    bool m_layoutResetQueued = false;

    AZStd::string m_filterString;
    AZStd::vector<ComponentTypeValue> m_componentFilters;
    bool m_isFilterDirty = true;

    void OnEntityCompositionChanged(const AzToolsFramework::EntityIdList& entityIds) override;

    void OnEntityInitialized(const AZ::EntityId& entityId) override;
    void AfterEntitySelectionChanged(const AzToolsFramework::EntityIdList&, const AzToolsFramework::EntityIdList&) override;

    //! AzToolsFramework::EditorEntityInfoNotificationBus::Handler
    //! Get notifications when the EditorEntityInfo changes so we can update our model
    void OnEntityInfoResetBegin() override;
    void OnEntityInfoResetEnd() override;
    void OnEntityInfoUpdatedAddChildBegin(AZ::EntityId parentId, AZ::EntityId childId) override;
    void OnEntityInfoUpdatedAddChildEnd(AZ::EntityId parentId, AZ::EntityId childId) override;
    void OnEntityInfoUpdatedRemoveChildBegin(AZ::EntityId parentId, AZ::EntityId childId) override;
    void OnEntityInfoUpdatedRemoveChildEnd(AZ::EntityId parentId, AZ::EntityId childId) override;
    void OnEntityInfoUpdatedOrderBegin(AZ::EntityId parentId, AZ::EntityId childId, AZ::u64 index) override;
    void OnEntityInfoUpdatedOrderEnd(AZ::EntityId parentId, AZ::EntityId childId, AZ::u64 index) override;
    void OnEntityInfoUpdatedSelection(AZ::EntityId entityId, bool selected) override;
    void OnEntityInfoUpdatedLocked(AZ::EntityId entityId, bool locked) override;
    void OnEntityInfoUpdatedVisibility(AZ::EntityId entityId, bool visible) override;
    void OnEntityInfoUpdatedName(AZ::EntityId entityId, const AZStd::string& name) override;
    void OnEntityInfoUpdateSliceOwnership(AZ::EntityId entityId) override;
    void OnEntityInfoUpdatedUnsavedChanges(AZ::EntityId entityId) override;

    // Drag/Drop of components from Component Palette.
    bool dropMimeDataComponentPalette(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);

    // Drag/Drop of entities.
    bool canDropMimeDataForEntityIds(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const;
    bool dropMimeDataEntities(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);

    // Drag/Drop of assets from asset browser.
    using ComponentAssetPair = AZStd::pair<AZ::TypeId, AZ::Data::AssetId>;
    using ComponentAssetPairs = AZStd::vector<ComponentAssetPair>;
    using SliceAssetList = AZStd::vector<AZ::Data::AssetId>;
    void DecodeAssetMimeData(const QMimeData* data, ComponentAssetPairs& componentAssetPairs, SliceAssetList& sliceAssets) const;
    bool DropMimeDataAssets(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);
    bool CanDropMimeDataAssets(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const;

    QMap<int, QVariant> itemData(const QModelIndex &index) const;
    QVariant dataForAll(const QModelIndex& index, int role) const;
    QVariant dataForName(const QModelIndex& index, int role) const;
    QVariant dataForVisibility(const QModelIndex& index, int role) const;
    QVariant dataForLock(const QModelIndex& index, int role) const;
    QVariant dataForSortIndex(const QModelIndex& index, int role) const;

    //! Request a hierarchy expansion
    void ExpandAncestors(const AZ::EntityId& entityId);
    bool IsExpanded(const AZ::EntityId& entityId) const;
    AZStd::unordered_map<AZ::EntityId, bool> m_entityExpansionState;

    void RestoreDescendantExpansion(const AZ::EntityId& entityId);
    void RestoreDescendantSelection(const AZ::EntityId& entityId);

    bool IsFiltered(const AZ::EntityId& entityId) const;
    AZStd::unordered_map<AZ::EntityId, bool> m_entityFilteredState;

    bool HasSelectedDescendant(const AZ::EntityId& entityId) const;

    bool AreAllDescendantsSameLockState(const AZ::EntityId& entityId) const;
    bool AreAllDescendantsSameVisibleState(const AZ::EntityId& entityId) const;

    enum LayerProperty
    {
        Locked,
        Invisible
    };
    bool IsInLayerWithProperty(AZ::EntityId entityId, const LayerProperty& layerProperty) const;

    // These are needed until we completely disassociated selection control from the outliner state to
    // keep track of selection state before/during/after filtering and searching
    AzToolsFramework::EntityIdList m_unfilteredSelectionEntityIds;
    void CacheSelectionIfAppropriate();
    void RestoreSelectionIfAppropriate();
    bool ShouldOverrideUnfilteredSelection();

    EntityOutliner::DisplaySortMode m_sortMode;

private:
    QVariant GetEntityIcon(const AZ::EntityId& id) const;
    QVariant GetEntityTooltip(const AZ::EntityId& id) const;

    const char* circleIconColor = "#ff7b00";
    const int circleIconDiameter = 5;
    const int maskDiameter = 8;  
};

class OutlinerCheckBox : public QCheckBox
{
   Q_OBJECT

public:
    explicit OutlinerCheckBox(QWidget* parent = nullptr);

    void draw(QPainter* painter);
};

// Class used to identify the visibility checkbox element for styling purposes
class OutlinerVisibilityCheckBox : public OutlinerCheckBox
{
    Q_OBJECT
public:
    explicit OutlinerVisibilityCheckBox(QWidget* parent = nullptr);
};

// Class used to identify the lock checkbox element for styling purposes
class OutlinerLockCheckBox : public OutlinerCheckBox
{
    Q_OBJECT
public:
    explicit OutlinerLockCheckBox(QWidget* parent = nullptr);
};

/*!
* OutlinerItemDelegate exists to render custom item-types.
* Other item-types render in the default fashion.
*/
class OutlinerItemDelegate
    : public QStyledItemDelegate
{
public:
    AZ_CLASS_ALLOCATOR(OutlinerItemDelegate, AZ::SystemAllocator, 0);

    OutlinerItemDelegate(QWidget* parent = nullptr);

    QString GetColumnHighlightedStylesheet(int column, bool highlighted) const;

    // Qt overrides
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    // The layer stripe is a continuous line from the layer's color box to the last entity in the layer.
    // Two layer stripes are drawn, one in the color of the layer and other in the border box color.
    void DrawLayerStripeAndBorder(QPainter* painter, int stripeX, int top, int bottom, QColor layerBorderColor, QColor layerColor) const;

    // Draws all UI related to layers for the current row.
    void DrawLayerUI(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index, const AZ::EntityId& entityId, bool isSelected) const;

    // Layers with unsaved changes, and layers with errors, have additional text added to their strings.
    QString GetLayerInfoString(const AZ::EntityId& entityId) const;

    // Entity names are offset vertically if they are in a layer, and generally to better line up with icons.
    int GetEntityNameVerticalOffset(const AZ::EntityId& entityId) const;

    // Mutability added because these are being used ONLY as renderers
    // for custom check boxes. The decision of whether or not to draw
    // them checked is tracked by the individual entities and items in
    // the hierarchy cache.
    mutable OutlinerVisibilityCheckBox m_visibilityCheckBox;
    mutable OutlinerVisibilityCheckBox m_visibilityCheckBoxWithBorder;
    mutable OutlinerVisibilityCheckBox m_visibilityCheckBoxLayerOverride;
    mutable OutlinerLockCheckBox m_lockCheckBox;
    mutable OutlinerLockCheckBox m_lockCheckBoxWithBorder;
    mutable OutlinerLockCheckBox m_lockCheckBoxLayerOverride;

    const int m_layerDividerLineHeight = 1;
    const int m_lastEntityInLayerDividerLineHeight = 1;

    QColor m_outlinerSelectionColor;

    OutlinerCheckBox* setupCheckBox(const QStyleOptionViewItem& option, const QModelIndex& index, const QColor& backgroundColor, bool isLayerEntity) const;
};

Q_DECLARE_METATYPE(AZ::ComponentTypeList); // allows type to be stored by QVariable

#endif
