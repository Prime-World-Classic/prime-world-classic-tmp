package Training
{
	import BaseClasses.BaseIconLoader;
	import flash.display.DisplayObjectContainer;
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.events.KeyboardEvent;
	import flash.events.MouseEvent;
	import flash.text.TextField;
	import flash.text.TextFieldType;
	import flash.ui.Keyboard;

	// Окно выбора панели тренировки (герой, скин, талант из библиотеки): сетка иконок с поиском по имени,
	// как список героев в замке лаунчера. Текущий отмечен золотой рамкой, у остальных рамка своего цвета
	// (редкость таланта) или тёмная; имя под курсором — внизу окна. Больше 10×7 иконок — прокрутка колесом.
	// Нажатие на иконку выбирает и закрывает окно; закрывают и крестик, Escape и нажатие мимо окна. Пока
	// окно открыто, горячие клавиши игры выключены (поиск печатается, а не жмёт таланты).
	//
	// Ячейки — постоянный набор на видимую область: движок не освобождает спрайты (ассерт «Sprite count > 6000»),
	// поэтому при смене списка, поиске и прокрутке ячейкам только назначаются другие элементы списка.
	public class TrainingGrid
	{
		private static const COLS:int = 10;
		private static const ICON:Number = 52;
		private static const GAP:Number = 8;
		private static const PAD:Number = 16;
		private static const HEADER_H:Number = 44;
		private static const FOOTER_H:Number = 30;
		private static const MAX_ROWS:int = 7;
		private static const SEARCH_W:Number = 200;
		private static const FRAME_COLOR:uint = 0x000000;

		private var parent:DisplayObjectContainer;
		private var onSelect:Function;
		private var root:Sprite;
		private var cover:MovieClip;
		private var window:MovieClip;
		private var back:Shape;
		private var titleText:TextField;
		private var searchField:TextField;
		private var searchHint:TextField;
		private var hoverText:TextField;
		private var closeButton:MovieClip;
		private var cellsHolder:Sprite;
		private var cells:Array = []; // постоянные ячейки, по одной на место сетки

		private var ids:Array = [];
		private var names:Array = [];
		private var icons:Array = [];
		private var colors:Array = null; // цвет рамки элемента (null — тёмная у всех)
		private var current:String = "";
		private var order:Array = [];    // индексы элементов в порядке показа
		private var filtered:Array = []; // подходящие под поиск, по порядку
		private var scrollRow:int = 0;
		private var rows:int = 1;        // строк в окне (по самому длинному списку, не меняется при поиске)
		private var shown:Boolean = false;

		public function TrainingGrid(_parent:DisplayObjectContainer)
		{
			parent = _parent;
		}

		public function get IsShown():Boolean
		{
			return shown;
		}

		// ids — то, что уходит в _onSelect(id); names и icons — подписи и пути текстур; _colors — цвета рамок
		// (необязательно); sortByName — по алфавиту, иначе в порядке списка
		public function Show(title:String, _ids:Array, _names:Array, _icons:Array, _current:String, _onSelect:Function,
			_colors:Array = null, sortByName:Boolean = true):void
		{
			// до назначения иконок: загрузчики читают текстуры сразу, клиент на это время снимает
			// ассерты о загрузке после старта
			if (!shown)
				FSCommands.Send(TrainingPanel.COMMAND, "grid 1");

			if (root == null)
				Create();
			onSelect = _onSelect;

			titleText.text = title;
			searchHint.text = Localization.Training_Search;
			searchField.text = "";
			searchHint.visible = true;
			hoverText.text = "";

			ids = _ids != null ? _ids.concat() : [];
			names = _names != null ? _names.concat() : [];
			icons = _icons != null ? _icons.concat() : [];
			colors = _colors != null ? _colors.concat() : null;
			current = _current;

			order = [];
			for (var i:int = 0; i < ids.length; i++)
				order.push(i);
			if (sortByName)
				order.sort(function(a:int, b:int):int
				{
					var na:String = String(names[a]).toLowerCase();
					var nb:String = String(names[b]).toLowerCase();
					return na < nb ? -1 : (na > nb ? 1 : 0);
				});
			rows = Math.max(1, Math.min(MAX_ROWS, Math.ceil(ids.length / COLS)));
			scrollRow = 0;
			Filter();

			if (!shown)
			{
				shown = true;
				parent.addChild(root);
				root.addEventListener(Event.ENTER_FRAME, OnEnterFrame);
				FSCommands.Send(FSCommands.ESCMENU_DISABLEHOTKEYS, FSCommands.ENABLE);
			}
			Layout();
			if (root.stage != null)
				root.stage.focus = searchField;
		}

		public function Hide():void
		{
			if (!shown)
				return;
			shown = false;
			root.removeEventListener(Event.ENTER_FRAME, OnEnterFrame);
			if (root.stage != null && root.stage.focus == searchField)
				root.stage.focus = null;
			if (root.parent != null)
				root.parent.removeChild(root);
			FSCommands.Send(FSCommands.ESCMENU_DISABLEHOTKEYS, FSCommands.DISABLE);
			FSCommands.Send(TrainingPanel.COMMAND, "grid 0");
		}

		private function Create():void
		{
			root = new Sprite();

			// затемнение экрана: нажатие мимо окна закрывает его
			cover = new MovieClip();
			var coverShape:Shape = new Shape();
			coverShape.name = "shape";
			cover.addChild(coverShape);
			TrainingUI.OnPress(cover, Hide);
			cover.buttonMode = false;
			root.addChild(cover);

			window = new MovieClip();
			window.addEventListener(MouseEvent.MOUSE_DOWN, TrainingUI.StopEvent);
			window.addEventListener(MouseEvent.MOUSE_UP, TrainingUI.StopEvent);
			window.addEventListener(MouseEvent.CLICK, TrainingUI.StopEvent);
			window.addEventListener(MouseEvent.DOUBLE_CLICK, TrainingUI.StopEvent);
			window.addEventListener(MouseEvent.MOUSE_WHEEL, OnWheel);
			root.addChild(window);

			back = new Shape();
			window.addChild(back);

			titleText = TrainingUI.CreateText(17, TrainingUI.ACCENT, 520);
			titleText.x = PAD;
			titleText.y = 12;
			window.addChild(titleText);

			// поиск
			var searchBack:Shape = new Shape();
			searchBack.name = "searchBack";
			TrainingUI.DrawRect(searchBack, 0x000000, 0.6, SEARCH_W, 24, 6);
			window.addChild(searchBack);

			searchHint = TrainingUI.CreateText(12, TrainingUI.TEXT_DIM, SEARCH_W - 12);
			window.addChild(searchHint);

			searchField = TrainingUI.CreateText(12, TrainingUI.TEXT, SEARCH_W - 12);
			searchField.type = TextFieldType.INPUT;
			searchField.selectable = true;
			searchField.mouseEnabled = true;
			searchField.maxChars = 24;
			searchField.addEventListener(Event.CHANGE, OnSearchChange);
			searchField.addEventListener(KeyboardEvent.KEY_UP, OnSearchKey);
			window.addChild(searchField);

			closeButton = TrainingUI.CreateButton("×", 24, 24, Hide, 16);
			window.addChild(closeButton);

			cellsHolder = new Sprite();
			cellsHolder.x = PAD;
			cellsHolder.y = HEADER_H;
			window.addChild(cellsHolder);
			for (var i:int = 0; i < COLS * MAX_ROWS; i++)
				cells.push(CreateCell(i));

			hoverText = TrainingUI.CreateText(13, TrainingUI.TEXT, 600);
			window.addChild(hoverText);
		}

		// Ячейка сетки: в name — индекс показанного элемента списка ("" — пусто)
		private function CreateCell(place:int):MovieClip
		{
			var cell:MovieClip = new MovieClip();
			cell.mouseChildren = false;
			cell.name = "";
			cell.x = (place % COLS) * (ICON + GAP);
			cell.y = int(place / COLS) * (ICON + GAP);

			var frame:Shape = new Shape();
			frame.name = "frame";
			cell.addChild(frame);

			var icon:BaseIconLoader = new BaseIconLoader();
			icon.name = "icon";
			icon.mouseEnabled = false;
			icon.mouseChildren = false;
			icon.SetIconSize(ICON, ICON);
			cell.addChild(icon);

			var highlight:Shape = new Shape();
			highlight.name = "highlight";
			TrainingUI.DrawRect(highlight, 0xFFFFFF, 0.25, ICON, ICON, 6);
			highlight.visible = false;
			cell.addChild(highlight);

			cell.addEventListener(MouseEvent.MOUSE_OVER, function(e:MouseEvent):void
			{
				if (cell.name == "")
					return;
				highlight.visible = true;
				hoverText.text = String(names[int(cell.name)]);
				TrainingUI.CenterText(hoverText, 0, WindowWidth());
			});
			cell.addEventListener(MouseEvent.MOUSE_OUT, function(e:MouseEvent):void
			{
				highlight.visible = false;
				UpdateFooter();
			});
			TrainingUI.OnPress(cell, function():void
			{
				if (cell.name == "")
					return;
				var id:String = String(ids[int(cell.name)]);
				Hide();
				if (onSelect != null)
					onSelect(id);
			});
			return cell;
		}

		// Ячейке — элемент списка (или пусто): иконка перезагружается только при смене пути
		private function AssignCell(cell:MovieClip, index:int):void
		{
			if (index < 0)
			{
				cell.name = "";
				if (cell.parent != null)
					cellsHolder.removeChild(cell);
				return;
			}

			cell.name = String(index);
			var icon:BaseIconLoader = cell.getChildAt(1) as BaseIconLoader; // рамка, иконка, подсветка; name иконки — её путь
			var path:String = String(icons[index]);
			if (icon.name != "icon:" + path)
			{
				icon.name = "icon:" + path;
				icon.SetIcon(path);
			}
			DrawFrame(cell);
			if (cell.parent == null)
				cellsHolder.addChild(cell);
		}

		// Рамка ячейки: золотая у текущего, иначе цвет элемента (редкость) или тёмная
		private function DrawFrame(cell:MovieClip):void
		{
			var index:int = int(cell.name);
			var isCurrent:Boolean = String(ids[index]) == current;
			var color:uint = FRAME_COLOR;
			var alpha:Number = 0.5;
			if (isCurrent)
			{
				color = TrainingUI.ACCENT;
				alpha = 1;
			}
			else if (colors != null && index < colors.length)
			{
				color = uint(colors[index]);
				alpha = 0.9;
			}
			TrainingUI.DrawRect(cell.getChildByName("frame") as Shape, color, alpha, ICON + 6, ICON + 6, 10, -3, -3);
		}

		private function OnSearchChange(e:Event):void
		{
			searchHint.visible = searchField.text.length == 0;
			scrollRow = 0;
			Filter();
		}

		private function OnSearchKey(e:KeyboardEvent):void
		{
			e.stopPropagation();
			if (e.keyCode == Keyboard.ESCAPE)
				Hide();
			else if (e.keyCode == Keyboard.ENTER && filtered.length == 1)
			{
				// Enter выбирает, если под поиск подходит ровно один
				var id:String = String(ids[filtered[0]]);
				Hide();
				if (onSelect != null)
					onSelect(id);
			}
		}

		private function OnWheel(e:MouseEvent):void
		{
			e.stopPropagation();
			var maxRow:int = Math.max(0, Math.ceil(filtered.length / COLS) - rows);
			var next:int = Math.max(0, Math.min(maxRow, scrollRow + (e.delta > 0 ? -1 : 1)));
			if (next == scrollRow)
				return;
			scrollRow = next;
			Render();
		}

		// Элементы, подходящие под поиск, по порядку
		private function Filter():void
		{
			var query:String = searchField.text.toLowerCase();
			filtered = [];
			for (var i:int = 0; i < order.length; i++)
			{
				var index:int = order[i];
				if (query.length == 0 || String(names[index]).toLowerCase().indexOf(query) >= 0)
					filtered.push(index);
			}
			Render();
		}

		private function Render():void
		{
			for (var place:int = 0; place < cells.length; place++)
			{
				var at:int = scrollRow * COLS + place;
				AssignCell(cells[place], place < rows * COLS && at < filtered.length ? filtered[at] : -1);
			}
			UpdateFooter();
		}

		// Внизу — имя под курсором, а без него при длинном списке — какие строки видны
		private function UpdateFooter():void
		{
			var total:int = Math.ceil(filtered.length / COLS);
			if (total > rows)
				hoverText.text = (scrollRow + 1) + "–" + Math.min(total, scrollRow + rows) + " / " + total + "  ↕";
			else
				hoverText.text = "";
			TrainingUI.CenterText(hoverText, 0, WindowWidth());
		}

		private function WindowWidth():Number
		{
			return PAD * 2 + COLS * ICON + (COLS - 1) * GAP;
		}

		private function WindowHeight():Number
		{
			return HEADER_H + rows * ICON + (rows - 1) * GAP + FOOTER_H;
		}

		private function OnEnterFrame(e:Event):void
		{
			Layout();
		}

		private function Layout():void
		{
			if (root == null || root.stage == null)
				return;

			var w:Number = WindowWidth();
			var h:Number = WindowHeight();
			var stageW:Number = root.stage.stageWidth;
			var stageH:Number = root.stage.stageHeight;

			TrainingUI.DrawRect(cover.getChildByName("shape") as Shape, 0x000000, 0.35, stageW, stageH, 0, -parent.x, -parent.y);
			TrainingUI.DrawRect(back, TrainingUI.PANEL_COLOR, 0.94, w, h, 14);

			window.x = Math.round((stageW - w) / 2 - parent.x);
			window.y = Math.round((stageH - h) / 2 - parent.y);

			closeButton.x = w - PAD - 24;
			closeButton.y = 10;

			var searchBack:Shape = window.getChildByName("searchBack") as Shape;
			searchBack.x = closeButton.x - 10 - SEARCH_W;
			searchBack.y = 10;
			searchField.x = searchHint.x = searchBack.x + 6;
			searchField.y = searchHint.y = searchBack.y + Math.round((24 - searchField.height) / 2);

			hoverText.y = h - FOOTER_H + Math.round((FOOTER_H - hoverText.height) / 2) - 2;
		}
	}
}
