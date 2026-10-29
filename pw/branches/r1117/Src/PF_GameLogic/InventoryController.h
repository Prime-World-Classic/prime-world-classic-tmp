#pragma once

#include "../UI/FSCommandListner.h"

namespace NWorld
{
_interface IInventory;
class      PFConsumable;
}

namespace UI            
{ 
  class FlashContainer2; 
  class ImageLabel;
}

namespace ActionBarSlotState
{
  enum EActionBarSlotState;
}

namespace NGameX
{
class AdventureFlashInterface;
class AdventureTooltip;
class VoiceOverlayController;
_interface IUIEventHandler;

class InventoryController : public UI::IFSCommandListner, public BaseObjectST
{
  NI_DECLARE_REFCOUNT_CLASS_2( InventoryController, UI::IFSCommandListner, BaseObjectST );

public:
  InventoryController(UI::FlashContainer2* flashContainer, UI::ImageLabel * tooltipWnd, AdventureFlashInterface * _flashInterface, IUIEventHandler * _eventHandler, const NWorld::IInventory * _inventory);
  ~InventoryController();

  void Update(int mouseX, int mouseY);

  // from UI::IFSCommandListner
  virtual void OnFSCommand( UI::FlashContainer2* _wnd, const char* listenerID,  const char* args, const wchar_t * argsW  );

  void SetTargetingItem(int _targetingConsumable) {targetingConsumable = _targetingConsumable;}
  void DropTarget();

  // Buttons of the action bar the player chose for consumables in the launcher: a new item goes into the first
  // free one of its buttons, and other items keep away from them while there are other free buttons
  void SetLauncherBridge( VoiceOverlayController * bridge ) { launcherBridge = bridge; }

private:
  void AddToActionBar( int index, const NWorld::PFConsumable * consumable );

  struct InventorySlotStatus
  {
    InventorySlotStatus();
    InventorySlotStatus(int _count, ActionBarSlotState::EActionBarSlotState _slotState, float _cooldown, float _maxCooldown);
    ActionBarSlotState::EActionBarSlotState slotState;
    int count;
    float cooldown;
    float maxCooldown;
    bool operator==(const InventorySlotStatus &other) const;
  };

  const InventorySlotStatus emptySlotStatus;

  InventorySlotStatus GetItemStatus( const NWorld::PFConsumable * consumable );
  void ShowConsumableDesc(int index, bool show);

  ConstPtr<NWorld::IInventory>      inventory;

  Weak<UI::FlashContainer2> flashWnd;
  
  int targetingConsumable;

  CObj<AdventureTooltip>        tooltip;
  
  vector<InventorySlotStatus>   slotStatuses;

  Weak<AdventureFlashInterface>          flashInterface;
  Weak<IUIEventHandler>         eventHandler;
  Weak<VoiceOverlayController>  launcherBridge;

};

}//namespace NGameX