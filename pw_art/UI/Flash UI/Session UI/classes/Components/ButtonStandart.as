package Components
{

	import Controllers.SoundController;
	import flash.display.MovieClip;
	import flash.display.Bitmap;
	import flash.display.BitmapData;
	import flash.display.DisplayObject;
	import flash.geom.Rectangle;
	import flash.text.TextFieldAutoSize;

	import src.ScaleBitmap;
	import flash.text.TextField;
	import src.WButton;
	import src.Desaturate;
	import flash.events.Event;
	
	public class ButtonStandart extends MovieClip
	{
		private var isLocalized:Boolean = false;
		private var originalWidth:Number;
		private var originalHeight:Number;
		private var hwSkinned:Boolean = false;
		private var hwLabelColor:uint = 0xFFFFFF;
		private var hwSelected:Bitmap;
		private var hwBaseColor:uint = 0xFFFFFF;
		//reference
		public var label_txt:TextField;
		public var button_mc:WButton;

		public function ButtonStandart()
		{
			label_txt.height = label_txt.height-2;
			label_txt.mouseEnabled = false;
			label_txt.autoSize = TextFieldAutoSize.CENTER;
			
			var xOffset:int = 10;
			var yOffset:int = 7;
			
			var rec:Rectangle = new Rectangle(xOffset, yOffset, Math.max(0, int(button_mc.width - xOffset * 2)), Math.max(0, int(button_mc.height - yOffset * 2)));
			
			button_mc.ConvertBitmapToScaleBitmap(rec);
			
			configUI();
			
			button_mc.addEventListener("Up", OnUp);

			ApplySkin();
		}

		// halloween skin: plank a bit larger than the button
		private function ApplySkin():void
		{
			try
			{
				if (!Skin.Active || button_mc.UpVisObj == null)
					return;
				var up:DisplayObject = button_mc.UpVisObj;
				var r:Rectangle = new Rectangle(up.x, up.y, up.width, up.height);
				r.inflate(4, 3);
				if (!button_mc.SkinStates(["HW_BtnUp", "HW_BtnOver", "HW_BtnDown", "HW_BtnDisable"], r))
					return;
				hwLabelColor = label_txt.textColor;
				hwBaseColor = hwLabelColor;
				hwSkinned = true;
				// "selected" look (category of the options window): the active plank over the normal one
				hwSelected = Skin.Bmp("HW_BtnDown", r.width, r.height);
				if (hwSelected != null)
				{
					hwSelected.x = r.x;
					hwSelected.y = r.y;
					hwSelected.visible = false;
					up = button_mc.UpVisObj;
					button_mc.addChildAt(hwSelected, button_mc.getChildIndex(up) + 1);
				}
			}
			catch (e:Error)
			{
				Skin.Fail("ButtonStandart", e);
			}
		}

		// halloween skin only: shows the button as the selected one
		public function set Selected(value:Boolean):void
		{
			if (hwSelected != null)
				hwSelected.visible = value;
			// the label of the selected button is orange (set last: the component parameters reset the colour)
			if (hwSkinned)
			{
				hwLabelColor = value ? Skin.SELECTED_TEXT : hwBaseColor;
				label_txt.textColor = hwLabelColor;
				// the game's player redraws the text only when it changes
				var t:String = label_txt.text;
				label_txt.text = "";
				label_txt.text = t;
				label_txt.textColor = hwLabelColor;
			}
		}

		public function get IsSkinned():Boolean
		{
			return hwSkinned;
		}
		
		private function OnUp(e:Event):void 
		{
			SoundController.PlaySound(SoundController.BUTTON_PRESS_BIG);
		}

		private function configUI():void
		{
			var r:Number = rotation;
			rotation = 0;
			originalWidth = super.width;
			originalHeight = super.height;
			super.scaleX = super.scaleY = 1;
			setSize(originalWidth,originalHeight);
			rotation = r;
		}
		
		private function setSize(_width:Number, _height:Number):void
		{
			button_mc.SetScaleBitmapSizes(_width, _height);
			
			label_txt.x = (button_mc.UpVisObj.width - label_txt.width)/2;
			label_txt.y = (button_mc.UpVisObj.height - (label_txt.height-2))/2;
		}
		
		public function set LocalizeText(value:String):void 
		{		
			label_txt.text = value;
			isLocalized = true;
			// the game's player resets the colour with the text
			if (hwSkinned)
				label_txt.textColor = hwLabelColor;
		}

		public function set labelColor(_color:uint)
		{
			hwLabelColor = _color;
			label_txt.textColor = _color;
		}

		public function get labelColor():uint
		{
			return label_txt.textColor;
		}
		public function set labelText(_text:String)
		{
			if (isLocalized) return;
			label_txt.text = _text;
			if (hwSkinned)
				label_txt.textColor = hwLabelColor;
		}
		public function get labelText():String
		{
			return label_txt.text;
		}

		public function disable():void
		{
			mouseChildren = false;
				
			button_mc.UpVisObj.visible = false;
			button_mc.OverVisObj.visible = false;
			button_mc.DownVisObj.visible = false;
			button_mc.DisableVisObj.visible = true;
			
			Desaturate.desaturate(button_mc);
		}

		public function enable():void
		{
			mouseChildren = true;
			button_mc.UpVisObj.visible = true;
			button_mc.OverVisObj.visible = false;
			button_mc.DownVisObj.visible = false;
			button_mc.DisableVisObj.visible = false;
			Desaturate.saturate(button_mc);
		}
		
		public function set EnableButton(value:Boolean):void
		{
			if (value)
				enable()
			else
				disable()
		}
		
		public function GetHeight():Number
		{
			return originalHeight;
		}
		
		public function GetWidth():Number 
		{
			return originalWidth
		}
	}
}