package Training
{
	import BaseClasses.BaseIconLoader;
	import BaseClasses.TooltipSender;
	import flash.display.DisplayObjectContainer;
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.events.MouseEvent;
	import flash.text.TextField;

	// Панель тренировки (сборки с читами): герой и скин, таланты, режимы, восстановление.
	// Стоит у левого края экрана; язычок справа от панели сворачивает её к краю (остаётся только язычок).
	// Данные шлёт клиент (PF_GameLogic/TrainingPanelController.cpp) через MainInterface.SetTrainingHeroes /
	// SetTrainingState; нажатия уходят FSCommand'ом COMMAND и выполняются читами локального героя.
	public class TrainingPanel
	{
		public static const COMMAND:String = "TrainingPanelCmd";

		private static const PANEL_W:Number = 232;
		private static const PAD:Number = 12;
		private static const INNER_W:Number = PANEL_W - PAD * 2;
		private static const AVATAR:Number = 46;
		private static const BUTTON_H:Number = 24;
		private static const TAB_W:Number = 18;
		private static const TAB_H:Number = 64;
		private static const TOP:Number = 150; // ниже портретов команд

		private static const MODES:Array = ["NoDamage", "NoCooldown", "NoMana"];
		private static const REMOVE_W:Number = 70;
		private static const ALLY_COLOR:uint = 0x4FCB5A;
		private static const ENEMY_COLOR:uint = 0xE0564B;
		// рамки библиотеки по редкости таланта (NDb::ETalentRarity: классовый, обычный, хороший, превосходный,
		// великолепный, эксклюзивный, выдающийся)
		private static const RARITY_COLORS:Array = [0x3FA9F5, 0x9E9E9E, 0x4FCB5A, 0x3D7BFF, 0xA74BFF, 0xFF8A1E, 0xE0564B];

		// Библиотека талантов: правый клик по таланту в билде (TalentBar/Talent.as), пока панель показана
		public static var LibraryEnabled:Boolean = false;

		public static function OpenLibrary(column:int, row:int):void
		{
			if (LibraryEnabled)
				FSCommands.Send(COMMAND, "library " + column + " " + row);
		}

		private var parent:DisplayObjectContainer;
		private var root:Sprite;
		private var panel:MovieClip;
		private var panelBack:Shape;
		private var panelH:Number = 0;
		private var tab:MovieClip;
		private var tabArrow:TextField;
		private var tabTooltip:TooltipSender;
		private var heroGrid:TrainingGrid; // свои ячейки у списка героев и у списка скинов: не пересоздаются
		private var skinGrid:TrainingGrid;

		private var titleText:TextField;
		private var heroIcon:BaseIconLoader;
		private var heroName:TextField;
		private var heroButton:MovieClip;
		private var skinIcon:BaseIconLoader;
		private var skinName:TextField;
		private var skinButton:MovieClip;
		private var talentsHeader:TextField;
		private var talentsCount:TextField;
		private var randomTalentButton:MovieClip;
		private var randomTalentTooltip:TooltipSender;
		private var libraryHint:TextField;
		private var allTalentsButton:MovieClip;
		private var modesHeader:TextField;
		private var modeBoxes:Array = [];
		private var refreshButton:MovieClip;
		private var refreshTooltip:TooltipSender;
		private var unitsHeader:TextField;
		private var addAllyButton:MovieClip;
		private var addEnemyButton:MovieClip;
		private var addDummyButton:MovieClip;
		private var addTooltips:Array = [];
		private var selectedText:TextField;
		private var removeButton:MovieClip;
		private var removeTooltip:TooltipSender;
		private var controlButton:MovieClip;
		private var controlTooltip:TooltipSender;
		private var releaseButton:MovieClip;
		private var releaseTooltip:TooltipSender;
		private var dummyHeader:TextField;
		private var dummyResetButton:MovieClip;
		private var dummyLabels:Array = [];
		private var dummyValues:Array = [];
		private var selectedName:String = "";

		private var shown:Boolean = false;
		private var collapsed:Boolean = false;

		// Данные от клиента
		private var heroIds:Array = [];
		private var heroNames:Array = [];
		private var heroIcons:Array = [];
		private var heroIndex:int = -1;
		private var skinIds:Array = [];
		private var skinNames:Array = [];
		private var skinIcons:Array = [];
		private var currentSkin:int = 0;
		private var modes:int = 0;
		private var lastStageH:Number = -1;
		private var lastParentX:Number = NaN;

		public function TrainingPanel(_parent:DisplayObjectContainer)
		{
			parent = _parent;
		}

		// Список библиотеки для слота билда (клиент, после правого клика): классовые таланты героя этого ряда,
		// потом общие, редкие первыми; выбор меняет талант в слоте прямо в бою
		public function ShowLibrary(level:int, slot:int, ids:Array, names:Array, icons:Array, rarities:Array, current:String):void
		{
			if (root == null)
				Create();
			var frameColors:Array = [];
			for (var i:int = 0; i < (rarities != null ? rarities.length : 0); i++)
			{
				var rarity:int = int(rarities[i]);
				frameColors.push(rarity >= 0 && rarity < RARITY_COLORS.length ? RARITY_COLORS[rarity] : 0x000000);
			}
			heroGrid.Hide();
			skinGrid.Show(Localization.Training_LibraryTitle.replace("{0}", String(level + 1)), ids, names, icons, current,
				function(id:String):void { Send("set_talent " + level + " " + slot + " " + id); }, frameColors, false);
		}

		public function Show(show:Boolean, _collapsed:Boolean):void
		{
			if (show == shown)
				return;
			shown = show;
			LibraryEnabled = show;

			if (show)
			{
				if (root == null)
					Create();
				collapsed = _collapsed;
				UpdateTexts();
				ApplyCollapsed();
				parent.addChild(root);
				root.addEventListener(Event.ENTER_FRAME, OnEnterFrame);
				lastStageH = -1;
				Layout();
			}
			else if (root != null)
			{
				if (heroGrid != null)
					heroGrid.Hide();
				if (skinGrid != null)
					skinGrid.Hide();
				root.removeEventListener(Event.ENTER_FRAME, OnEnterFrame);
				if (root.parent != null)
					root.parent.removeChild(root);
			}
		}

		public function SetHeroes(ids:Array, names:Array, icons:Array):void
		{
			heroIds = ids != null ? ids : [];
			heroNames = names != null ? names : [];
			heroIcons = icons != null ? icons : [];
			UpdateHero();
		}

		public function SetState(_heroIndex:int, learned:int, total:int, available:int, _modes:int,
			_skinIds:Array, _skinNames:Array, _skinIcons:Array, _currentSkin:int):void
		{
			heroIndex = _heroIndex;
			modes = _modes;
			skinIds = _skinIds != null ? _skinIds : [];
			skinNames = _skinNames != null ? _skinNames : [];
			skinIcons = _skinIcons != null ? _skinIcons : [];
			currentSkin = _currentSkin;

			if (root == null)
				Create();

			UpdateHero();

			talentsCount.text = learned + " / " + total;
			talentsCount.width = Math.ceil(talentsCount.textWidth) + 4;
			talentsCount.x = PANEL_W - PAD - talentsCount.width;
			TrainingUI.SetButtonEnabled(randomTalentButton, available > 0);
			TrainingUI.SetButtonEnabled(allTalentsButton, learned < total);

			for (var i:int = 0; i < modeBoxes.length; i++)
				TrainingUI.SetChecked(modeBoxes[i], (modes & (1 << i)) != 0);
		}

		private function Create():void
		{
			root = new Sprite();

			panel = new MovieClip();
			panel.addEventListener(MouseEvent.MOUSE_DOWN, TrainingUI.StopEvent);
			panel.addEventListener(MouseEvent.MOUSE_UP, TrainingUI.StopEvent);
			panel.addEventListener(MouseEvent.CLICK, TrainingUI.StopEvent);
			panel.addEventListener(MouseEvent.DOUBLE_CLICK, TrainingUI.StopEvent);
			panel.addEventListener(MouseEvent.MOUSE_WHEEL, TrainingUI.StopEvent);
			root.addChild(panel);

			panelBack = new Shape();
			panel.addChild(panelBack);

			var y:Number = 10;
			titleText = TrainingUI.CreateText(15, TrainingUI.ACCENT, INNER_W, true);
			titleText.x = PAD;
			titleText.y = y;
			panel.addChild(titleText);
			y += 30;

			// Герой: портрет, имя, «Сменить героя»
			heroIcon = CreateAvatar(PAD, y, OpenHeroGrid);
			heroName = TrainingUI.CreateText(13, TrainingUI.TEXT, INNER_W - AVATAR - 8, true);
			heroName.x = PAD + AVATAR + 8;
			heroName.y = y - 1;
			panel.addChild(heroName);
			heroButton = TrainingUI.CreateButton("", INNER_W - AVATAR - 8, BUTTON_H - 2, OpenHeroGrid, 11);
			heroButton.x = PAD + AVATAR + 8;
			heroButton.y = y + AVATAR - BUTTON_H + 2;
			panel.addChild(heroButton);
			y += AVATAR + 8;

			// Скин: иконка, имя, «Сменить скин»
			skinIcon = CreateAvatar(PAD, y, OpenSkinGrid);
			skinName = TrainingUI.CreateText(12, TrainingUI.TEXT, INNER_W - AVATAR - 8);
			skinName.x = PAD + AVATAR + 8;
			skinName.y = y;
			panel.addChild(skinName);
			skinButton = TrainingUI.CreateButton("", INNER_W - AVATAR - 8, BUTTON_H - 2, OpenSkinGrid, 11);
			skinButton.x = PAD + AVATAR + 8;
			skinButton.y = y + AVATAR - BUTTON_H + 2;
			panel.addChild(skinButton);
			y += AVATAR + 12;

			// Таланты
			talentsHeader = CreateHeader(y);
			talentsCount = TrainingUI.CreateText(12, TrainingUI.TEXT_DIM, 80);
			talentsCount.y = y + 1;
			panel.addChild(talentsCount);
			y += 22;
			var half:Number = (INNER_W - 6) / 2;
			randomTalentButton = TrainingUI.CreateButton("", half, BUTTON_H, function():void { Send("talent_next"); }, 11);
			randomTalentButton.x = PAD;
			randomTalentButton.y = y;
			panel.addChild(randomTalentButton);
			randomTalentTooltip = new TooltipSender(randomTalentButton);
			allTalentsButton = TrainingUI.CreateButton("", half, BUTTON_H, function():void { Send("talents_all"); }, 11);
			allTalentsButton.x = PAD + half + 6;
			allTalentsButton.y = y;
			panel.addChild(allTalentsButton);
			y += BUTTON_H + 4;
			libraryHint = TrainingUI.CreateText(10, TrainingUI.TEXT_DIM, INNER_W);
			libraryHint.x = PAD;
			libraryHint.y = y;
			panel.addChild(libraryHint);
			y += 18;

			// Режимы
			modesHeader = CreateHeader(y);
			y += 22;
			for (var i:int = 0; i < MODES.length; i++)
			{
				var box:MovieClip = TrainingUI.CreateCheckBox("", INNER_W, ModeToggler(i));
				box.x = PAD;
				box.y = y;
				panel.addChild(box);
				modeBoxes.push(box);
				y += 24;
			}
			y += 6;

			refreshButton = TrainingUI.CreateButton("", INNER_W, BUTTON_H + 4, function():void { Send("refresh"); }, 12, TrainingUI.ACCENT);
			refreshButton.x = PAD;
			refreshButton.y = y;
			panel.addChild(refreshButton);
			refreshTooltip = new TooltipSender(refreshButton);
			y += BUTTON_H + 4 + 16;

			// Юниты: герои на своей стороне, против неё и мишень (выбор героя той же сеткой), удаление выбранного
			unitsHeader = CreateHeader(y);
			y += 22;
			var third:Number = Math.floor((INNER_W - 12) / 3); // целая ширина: дробную движок не рисует
			addAllyButton = TrainingUI.CreateButton("", third, BUTTON_H, function():void { OpenAddGrid("ally"); }, 11, ALLY_COLOR);
			addEnemyButton = TrainingUI.CreateButton("", third, BUTTON_H, function():void { OpenAddGrid("enemy"); }, 11, ENEMY_COLOR);
			addDummyButton = TrainingUI.CreateButton("", third, BUTTON_H, function():void { OpenAddGrid("dummy"); }, 11);
			var addButtons:Array = [addAllyButton, addEnemyButton, addDummyButton];
			for (var k:int = 0; k < addButtons.length; k++)
			{
				var addButton:MovieClip = addButtons[k];
				addButton.x = Math.round(PAD + k * (third + 6));
				addButton.y = y;
				panel.addChild(addButton);
				addTooltips.push(new TooltipSender(addButton));
			}
			y += BUTTON_H + 8;
			selectedText = TrainingUI.CreateText(11, TrainingUI.TEXT_DIM, INNER_W - REMOVE_W - 6);
			selectedText.x = PAD;
			selectedText.y = y + 3;
			panel.addChild(selectedText);
			removeButton = TrainingUI.CreateButton("", REMOVE_W, BUTTON_H - 2, function():void { Send("remove"); }, 11, ENEMY_COLOR);
			removeButton.x = PANEL_W - PAD - REMOVE_W;
			removeButton.y = y;
			panel.addChild(removeButton);
			removeTooltip = new TooltipSender(removeButton);
			TrainingUI.SetButtonEnabled(removeButton, false);
			y += BUTTON_H + 4;

			// Управление: выбранным героем (бота или добавленным тренировкой) и возврат к своему
			var halfW:Number = Math.floor((INNER_W - 6) / 2);
			controlButton = TrainingUI.CreateButton("", halfW, BUTTON_H, function():void { Send("control"); }, 11);
			controlButton.x = PAD;
			controlButton.y = y;
			panel.addChild(controlButton);
			controlTooltip = new TooltipSender(controlButton);
			TrainingUI.SetButtonEnabled(controlButton, false);
			releaseButton = TrainingUI.CreateButton("", halfW, BUTTON_H, function():void { Send("release"); }, 11, TrainingUI.ACCENT);
			releaseButton.x = PANEL_W - PAD - halfW;
			releaseButton.y = y;
			panel.addChild(releaseButton);
			releaseTooltip = new TooltipSender(releaseButton);
			TrainingUI.SetButtonEnabled(releaseButton, false);
			y += BUTTON_H + 14;

			// Мишень: урон серии ударов, DPS, последний удар
			dummyHeader = CreateHeader(y);
			dummyResetButton = TrainingUI.CreateButton("", REMOVE_W, 18, function():void { Send("dummy_reset"); }, 10);
			dummyResetButton.x = PANEL_W - PAD - REMOVE_W;
			dummyResetButton.y = y - 1;
			panel.addChild(dummyResetButton);
			y += 22;
			for (var r:int = 0; r < 3; r++)
			{
				var label:TextField = TrainingUI.CreateText(12, TrainingUI.TEXT_DIM, INNER_W - 90);
				label.x = PAD;
				label.y = y;
				panel.addChild(label);
				dummyLabels.push(label);
				var value:TextField = TrainingUI.CreateText(13, TrainingUI.TEXT, 90);
				value.y = y - 1;
				panel.addChild(value);
				dummyValues.push(value);
				y += 19;
			}
			SetDummyValues(["—", "—", "—"]);
			y += PAD - 4;

			panelH = y;
			TrainingUI.DrawRect(panelBack, TrainingUI.PANEL_COLOR, TrainingUI.PANEL_ALPHA, PANEL_W + 12, panelH, 16, -12, 0);

			// Язычок у правого края панели: сворачивает и разворачивает
			tab = new MovieClip();
			tab.mouseChildren = false;
			var tabBack:Shape = new Shape();
			tabBack.name = "back";
			TrainingUI.DrawRect(tabBack, TrainingUI.PANEL_COLOR, TrainingUI.PANEL_ALPHA, TAB_W + 10, TAB_H, 12, -10, 0);
			tab.addChild(tabBack);
			tabArrow = TrainingUI.CreateText(16, TrainingUI.ACCENT, TAB_W, true);
			tabArrow.y = Math.round((TAB_H - tabArrow.height) / 2) - 1;
			tab.addChild(tabArrow);
			tab.addEventListener(MouseEvent.MOUSE_OVER, function(e:MouseEvent):void { TrainingUI.DrawRect(tabBack, 0x2A3340, 0.92, TAB_W + 10, TAB_H, 12, -10, 0); });
			tab.addEventListener(MouseEvent.MOUSE_OUT, function(e:MouseEvent):void { TrainingUI.DrawRect(tabBack, TrainingUI.PANEL_COLOR, TrainingUI.PANEL_ALPHA, TAB_W + 10, TAB_H, 12, -10, 0); });
			TrainingUI.OnPress(tab, ToggleCollapsed);
			tabTooltip = new TooltipSender(tab);
			tab.x = PANEL_W;
			tab.y = Math.round((panelH - TAB_H) / 2);
			root.addChild(tab);

			heroGrid = new TrainingGrid(parent);
			skinGrid = new TrainingGrid(parent);
		}

		// Выбранный юнит (удалить можно только добавленного тренировкой) и мишень: выбранная или последняя, по которой били
		public function SetUnits(_selectedName:String, canRemove:Boolean, hasDummy:Boolean, total:int, dps:int, last:int, hits:int,
			canControl:Boolean, controlling:Boolean):void
		{
			if (root == null)
				Create();
			selectedName = _selectedName != null ? _selectedName : "";
			UpdateSelectedText();
			TrainingUI.SetButtonEnabled(removeButton, canRemove);
			TrainingUI.SetButtonEnabled(controlButton, canControl);
			TrainingUI.SetButtonEnabled(releaseButton, controlling);
			TrainingUI.SetButtonEnabled(dummyResetButton, hasDummy && hits > 0);
			if (hasDummy && hits > 0)
				SetDummyValues([String(total), String(dps), String(last)]);
			else
				SetDummyValues(["—", "—", "—"]);
		}

		private function UpdateSelectedText():void
		{
			TrainingUI.FitText(selectedText, Localization.Training_Selected + " " + (selectedName.length > 0 ? selectedName : Localization.Training_NoSelection),
				INNER_W - REMOVE_W - 6);
		}

		private function SetDummyValues(values:Array):void
		{
			for (var i:int = 0; i < dummyValues.length && i < values.length; i++)
			{
				var value:TextField = dummyValues[i];
				value.text = values[i];
				value.width = Math.ceil(value.textWidth) + 4;
				value.x = PANEL_W - PAD - value.width;
			}
		}

		private function OpenAddGrid(side:String):void
		{
			if (heroIds.length == 0)
				return;
			skinGrid.Hide();
			heroGrid.Show(side == "ally" ? Localization.Training_AddAllyTitle : (side == "enemy" ? Localization.Training_AddEnemyTitle : Localization.Training_AddDummyTitle),
				heroIds, heroNames, heroIcons, "", function(id:String):void { Send("add " + id + " " + side); });
		}

		private function CreateAvatar(x:Number, y:Number, action:Function):BaseIconLoader
		{
			var holder:MovieClip = new MovieClip();
			holder.mouseChildren = false;
			holder.x = x;
			holder.y = y;
			var frame:Shape = new Shape();
			TrainingUI.DrawRect(frame, TrainingUI.ACCENT, 0.9, AVATAR + 4, AVATAR + 4, 10, -2, -2);
			holder.addChild(frame);
			var icon:BaseIconLoader = new BaseIconLoader();
			icon.mouseEnabled = false;
			icon.SetIconSize(AVATAR, AVATAR);
			holder.addChild(icon);
			TrainingUI.OnPress(holder, action);
			panel.addChild(holder);
			return icon;
		}

		private function CreateHeader(y:Number):TextField
		{
			var line:Shape = new Shape();
			TrainingUI.DrawRect(line, 0xFFFFFF, 0.12, INNER_W, 1, 0, PAD, y - 4);
			panel.addChild(line);
			var text:TextField = TrainingUI.CreateText(12, TrainingUI.TEXT_DIM, INNER_W, true);
			text.x = PAD;
			text.y = y;
			panel.addChild(text);
			return text;
		}

		private function ModeToggler(index:int):Function
		{
			return function():void
			{
				var on:Boolean = (modes & (1 << index)) == 0;
				// сразу на экране, клиент подтвердит следующим состоянием
				modes = on ? (modes | (1 << index)) : (modes & ~(1 << index));
				TrainingUI.SetChecked(modeBoxes[index], on);
				Send("mode " + MODES[index] + " " + (on ? 1 : 0));
			};
		}

		// Подписи из Localization (Combat.xdb, ru + en) — к показу панели переводы уже загружены
		private function UpdateTexts():void
		{
			titleText.text = Localization.Training_Title;
			TrainingUI.SetButtonLabel(heroButton, Localization.Training_ChangeHero, INNER_W - AVATAR - 8);
			TrainingUI.SetButtonLabel(skinButton, Localization.Training_ChangeSkin, INNER_W - AVATAR - 8);
			talentsHeader.text = Localization.Training_Talents;
			TrainingUI.SetButtonLabel(randomTalentButton, Localization.Training_RandomTalent, (INNER_W - 6) / 2);
			randomTalentTooltip.TooltipText = Localization.Training_RandomTalentTooltip;
			TrainingUI.FitText(libraryHint, Localization.Training_LibraryHint, INNER_W);
			TrainingUI.SetButtonLabel(allTalentsButton, Localization.Training_AllTalents, (INNER_W - 6) / 2);
			modesHeader.text = Localization.Training_Modes;
			var labels:Array = [Localization.Training_NoDamage, Localization.Training_NoCooldown, Localization.Training_NoMana];
			for (var i:int = 0; i < modeBoxes.length; i++)
			{
				var label:TextField = MovieClip(modeBoxes[i]).getChildAt(3) as TextField;
				if (label != null)
					label.text = labels[i];
			}
			TrainingUI.SetButtonLabel(refreshButton, Localization.Training_Refresh, INNER_W);
			refreshTooltip.TooltipText = Localization.Training_RefreshTooltip;

			unitsHeader.text = Localization.Training_Units;
			var third:Number = Math.floor((INNER_W - 12) / 3); // целая ширина: дробную движок не рисует
			TrainingUI.SetButtonLabel(addAllyButton, Localization.Training_AddAlly, third);
			TrainingUI.SetButtonLabel(addEnemyButton, Localization.Training_AddEnemy, third);
			TrainingUI.SetButtonLabel(addDummyButton, Localization.Training_AddDummy, third);
			var addTexts:Array = [Localization.Training_AddAllyTooltip, Localization.Training_AddEnemyTooltip, Localization.Training_AddDummyTooltip];
			for (var k:int = 0; k < addTooltips.length; k++)
				TooltipSender(addTooltips[k]).TooltipText = addTexts[k];
			TrainingUI.SetButtonLabel(removeButton, Localization.Training_Remove, REMOVE_W);
			removeTooltip.TooltipText = Localization.Training_RemoveTooltip;
			var halfW:Number = Math.floor((INNER_W - 6) / 2);
			TrainingUI.SetButtonLabel(controlButton, Localization.Training_Control, halfW);
			controlTooltip.TooltipText = Localization.Training_ControlTooltip;
			TrainingUI.SetButtonLabel(releaseButton, Localization.Training_Release, halfW);
			releaseTooltip.TooltipText = Localization.Training_ReleaseTooltip;
			UpdateSelectedText();

			dummyHeader.text = Localization.Training_Dummy;
			TrainingUI.SetButtonLabel(dummyResetButton, Localization.Training_DummyReset, REMOVE_W);
			var dummyTexts:Array = [Localization.Training_DummyDamage, Localization.Training_DummyDps, Localization.Training_DummyLast];
			for (var d:int = 0; d < dummyLabels.length; d++)
				TextField(dummyLabels[d]).text = dummyTexts[d];
			UpdateHero();
		}

		private function UpdateHero():void
		{
			if (root == null)
				return;

			var heroPos:int = HeroPosition();
			TrainingUI.FitText(heroName, heroPos >= 0 ? String(heroNames[heroPos]) : "", INNER_W - AVATAR - 8);
			SetIconOnce(heroIcon, heroPos >= 0 ? String(heroIcons[heroPos]) : "");

			var skinOk:Boolean = currentSkin >= 0 && currentSkin < skinIds.length;
			TrainingUI.FitText(skinName, skinOk ? String(skinNames[currentSkin]) : "", INNER_W - AVATAR - 8);
			SetIconOnce(skinIcon, skinOk ? String(skinIcons[currentSkin]) : "");
			TrainingUI.SetButtonEnabled(skinButton, skinIds.length > 1);
		}

		// Загрузчик перезагружает картинку при каждом SetIcon (новый спрайт) — только при смене пути
		private static function SetIconOnce(icon:BaseIconLoader, path:String):void
		{
			if (icon.name == "icon:" + path)
				return;
			icon.name = "icon:" + path;
			icon.SetIcon(path);
		}

		private function HeroPosition():int
		{
			for (var i:int = 0; i < heroIds.length; i++)
				if (int(heroIds[i]) == heroIndex)
					return i;
			return -1;
		}

		private function OpenHeroGrid():void
		{
			if (heroIds.length == 0)
				return;

			skinGrid.Hide();
			heroGrid.Show(Localization.Training_ChooseHero, heroIds, heroNames, heroIcons, String(heroIndex), function(id:String):void { Send("hero " + id); });
		}

		private function OpenSkinGrid():void
		{
			if (skinIds.length <= 1)
				return;

			heroGrid.Hide();
			skinGrid.Show(Localization.Training_ChooseSkin, skinIds, skinNames, skinIcons, String(skinIds[currentSkin]), function(id:String):void { Send("skin " + id); });
		}

		private function ToggleCollapsed():void
		{
			collapsed = !collapsed;
			ApplyCollapsed();
			Send("collapsed " + (collapsed ? 1 : 0));
		}

		private function ApplyCollapsed():void
		{
			panel.visible = !collapsed;
			tab.x = collapsed ? 0 : PANEL_W;
			tabArrow.text = collapsed ? "›" : "‹";
			TrainingUI.CenterText(tabArrow, 0, TAB_W);
			tabTooltip.TooltipText = collapsed ? Localization.Training_Expand : Localization.Training_Collapse;
		}

		// У левого края экрана, ниже портретов команд (координаты сцены — относительно родителя)
		private function OnEnterFrame(e:Event):void
		{
			Layout();
		}

		private function Layout():void
		{
			if (root == null || root.stage == null)
				return;
			var stageH:Number = root.stage.stageHeight;
			if (stageH == lastStageH && parent.x == lastParentX)
				return;
			lastStageH = stageH;
			lastParentX = parent.x;
			root.x = Math.round(-parent.x);
			root.y = Math.round(Math.max(0, Math.min(TOP, stageH - panelH - 200)) - parent.y);
		}

		private function Send(command:String):void
		{
			FSCommands.Send(COMMAND, command);
		}
	}
}
