package Training
{
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.events.MouseEvent;
	import flash.text.TextField;
	import flash.text.TextFieldAutoSize;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;

	// Общие элементы панели тренировки: тексты, кнопки, флажки.
	//
	// Движок PW: Sprite.graphics не рисуется (только дочерние Shape), обводок нет, TextField только без
	// масштаба, выравнивание текста по центру поля не работает, почти прозрачная заливка не ловит нажатие,
	// CLICK приходит не всегда — действие по MOUSE_DOWN, CLICK того же нажатия пропускается.
	public class TrainingUI
	{
		public static const FONT:String = "DejaVu LGC Sans";

		public static const PANEL_COLOR:uint = 0x0B0F14;
		public static const PANEL_ALPHA:Number = 0.82;
		public static const BUTTON_COLOR:uint = 0xFFFFFF;
		public static const BUTTON_ALPHA:Number = 0.12;
		public static const BUTTON_HOVER_ALPHA:Number = 0.26;
		public static const ACCENT:uint = 0xD9B44A;
		public static const TEXT:uint = 0xFFFFFF;
		public static const TEXT_DIM:uint = 0xA8A8A8;
		public static const ROUND:Number = 8;

		public static function CreateText(size:int, color:uint, fieldWidth:Number, bold:Boolean = false):TextField
		{
			var format:TextFormat = new TextFormat();
			format.font = FONT;
			format.size = size;
			format.color = color;
			// bold не задаём: у DejaVu LGC Sans в игре нет жирного начертания (ассерт «Can't set size for fonts»)
			format.align = TextFormatAlign.LEFT;

			var text:TextField = new TextField();
			text.selectable = false;
			text.mouseEnabled = false;
			text.multiline = false;
			text.wordWrap = false;
			text.autoSize = TextFieldAutoSize.NONE;
			text.defaultTextFormat = format;
			text.width = fieldWidth;
			text.height = size + 8;
			text.text = "";
			return text;
		}

		// Текст по центру отрезка [x, x + width] (выравнивание по центру поля движок не делает)
		public static function CenterText(text:TextField, x:Number, width:Number):void
		{
			text.width = Math.ceil(text.textWidth) + 4;
			text.x = Math.round(x + (width - text.width) / 2);
		}

		// Подрезает текст по ширине с многоточием
		public static function FitText(text:TextField, value:String, maxWidth:Number):void
		{
			text.text = value;
			if (text.textWidth <= maxWidth - 4)
				return;
			var cut:int = value.length;
			while (cut > 1)
			{
				--cut;
				text.text = value.substr(0, cut) + "…";
				if (text.textWidth <= maxWidth - 4)
					return;
			}
		}

		public static function DrawRect(shape:Shape, color:uint, alpha:Number, w:Number, h:Number, round:Number = ROUND, x:Number = 0, y:Number = 0):void
		{
			shape.graphics.clear();
			shape.graphics.beginFill(color, alpha);
			if (round > 0)
				shape.graphics.drawRoundRect(x, y, w, h, round, round);
			else
				shape.graphics.drawRect(x, y, w, h);
			shape.graphics.endFill();
		}

		// Нажатие: action по MOUSE_DOWN (или по CLICK, если MOUSE_DOWN не пришёл); события дальше в игру не идут
		public static function OnPress(target:MovieClip, action:Function):void
		{
			var handled:Boolean = false;
			target.buttonMode = true;
			target.addEventListener(MouseEvent.MOUSE_DOWN, function(e:MouseEvent):void
			{
				e.stopPropagation();
				handled = true;
				if (target.enabled)
					action();
			});
			target.addEventListener(MouseEvent.CLICK, function(e:MouseEvent):void
			{
				e.stopPropagation();
				if (!handled && target.enabled)
					action();
				handled = false;
			});
			target.addEventListener(MouseEvent.MOUSE_UP, StopEvent);
			target.addEventListener(MouseEvent.DOUBLE_CLICK, StopEvent);
		}

		public static function StopEvent(e:MouseEvent):void
		{
			e.stopPropagation();
		}

		// Кнопка с подписью по центру; подсветка при наведении
		public static function CreateButton(label:String, w:Number, h:Number, action:Function, size:int = 12, color:uint = BUTTON_COLOR):MovieClip
		{
			var button:MovieClip = new MovieClip();
			button.mouseChildren = false;

			var back:Shape = new Shape();
			back.name = "back";
			DrawRect(back, color, BUTTON_ALPHA, w, h);
			button.addChild(back);

			var text:TextField = CreateText(size, TEXT, w);
			text.name = "label";
			text.text = label;
			CenterText(text, 0, w);
			text.y = Math.round((h - text.height) / 2);
			button.addChild(text);

			button.addEventListener(MouseEvent.MOUSE_OVER, function(e:MouseEvent):void { if (button.enabled) DrawRect(back, color, BUTTON_HOVER_ALPHA, w, h); });
			button.addEventListener(MouseEvent.MOUSE_OUT, function(e:MouseEvent):void { DrawRect(back, color, BUTTON_ALPHA, w, h); });
			OnPress(button, action);
			return button;
		}

		public static function SetButtonLabel(button:MovieClip, label:String, w:Number):void
		{
			var text:TextField = button.getChildByName("label") as TextField;
			if (text == null)
				return;
			FitText(text, label, w - 8);
			CenterText(text, 0, w);
		}

		public static function SetButtonEnabled(button:MovieClip, enabled:Boolean):void
		{
			button.enabled = enabled;
			button.buttonMode = enabled;
			button.alpha = enabled ? 1 : 0.45;
		}

		// Флажок: квадрат и подпись; состояние — SetChecked
		public static function CreateCheckBox(label:String, w:Number, action:Function):MovieClip
		{
			var box:MovieClip = new MovieClip();
			box.mouseChildren = false;

			var hit:Shape = new Shape(); // строка целиком ловит нажатие (почти прозрачная заливка его не ловит)
			hit.name = "hit";
			DrawRect(hit, BUTTON_COLOR, 0.05, w, 22, 6);
			box.addChild(hit);

			var square:Shape = new Shape();
			square.name = "square";
			square.y = 3;
			DrawRect(square, BUTTON_COLOR, BUTTON_ALPHA * 1.5, 16, 16, 4);
			box.addChild(square);

			var mark:Shape = new Shape();
			mark.name = "mark";
			mark.x = 4;
			mark.y = 7;
			DrawRect(mark, ACCENT, 1, 8, 8, 3);
			mark.visible = false;
			box.addChild(mark);

			var text:TextField = CreateText(12, TEXT, w - 24);
			text.text = label;
			text.x = 22;
			text.y = Math.round((22 - text.height) / 2);
			box.addChild(text);

			box.addEventListener(MouseEvent.MOUSE_OVER, function(e:MouseEvent):void { DrawRect(square, BUTTON_COLOR, BUTTON_HOVER_ALPHA * 1.3, 16, 16, 4); });
			box.addEventListener(MouseEvent.MOUSE_OUT, function(e:MouseEvent):void { DrawRect(square, BUTTON_COLOR, BUTTON_ALPHA * 1.5, 16, 16, 4); });
			OnPress(box, action);
			return box;
		}

		public static function SetChecked(box:MovieClip, checked:Boolean):void
		{
			var mark:Shape = box.getChildByName("mark") as Shape;
			if (mark != null)
				mark.visible = checked;
		}
	}
}
