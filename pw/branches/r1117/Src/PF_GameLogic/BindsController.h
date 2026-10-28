#pragma once

namespace NGameX
{
  class BindsController : public BaseObjectST
  {
    NI_DECLARE_REFCOUNT_CLASS_1( BindsController, BaseObjectST );
  public: 
    enum BindType
    {
      None = 0,
      Chat = 1,
      ReportWindow = 2,
      EscMenu = 4,
      Lua = 8,
      TrainingGrid = 16 // the choice window of the training panel: its search field takes the keys
    };

    BindsController();
    ~BindsController(){};

    void SetEnabled(BindType type , bool enableBinds);
  private:
    bool IsFlagEnabled(BindType type);

    int currentEnabledFlags;
  };
}
