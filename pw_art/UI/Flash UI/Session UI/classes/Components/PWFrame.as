package Components 
{
	import flash.display.MovieClip;
	import flash.geom.Rectangle;
	import flash.display.BitmapData;
	import flash.utils.getDefinitionByName;
	import flash.utils.getQualifiedClassName;
	import src.ScaleBitmap;
	import flash.display.Bitmap;
	import flash.events.MouseEvent;

	public class PWFrame extends MovieClip
	{
		private var rectangle:Rectangle;
		protected var Width:Number;
		protected var Height:Number;
		protected var image:Bitmap;
		protected var scaleImagesArray:Array = [];
			
		public function PWFrame()
		{
			if(this.scale9Grid)
			{
				setRectangle();
				configure();
			}
		}
		public function configure():void
		{
				image = getChildAt(0) as Bitmap;			
				setScaleBitmap(image);
		}
		protected function setScaleBitmap(image:Bitmap)
		{			
			var depth:int = getChildIndex(image);
			var image_bd:BitmapData = image.bitmapData;

			// halloween skin: dark plate with an orange border for the value fields (same size and grid as textFrame.png)
			try
			{
				if (getQualifiedClassName(this) == "sliderFrame_cmp" && Skin.Has("HW_ValueFrame"))
					image_bd = Skin.Bmd("HW_ValueFrame");
			}
			catch (e:Error)
			{
				Skin.Fail("PWFrame", e);
			}

			var scaleBitmap:ScaleBitmap = new ScaleBitmap(image_bd);
			scaleBitmap.scale9Grid = rectangle;
				
			removeChild(getChildAt(depth));
			addChildAt(scaleBitmap, depth);
			scaleImagesArray.push(scaleBitmap);
			
			setSize(this.width, this.height);
		}
		public function setSize(_w:Number, _h:Number):void
		{
			Width = _w;
			Height = _h;
			
			for(var i:int=0; i<scaleImagesArray.length; i++)
			{
				scaleImagesArray[i].width = Width;
				scaleImagesArray[i].height = Height;
			}
			reSize();
			
			this.scaleX = 1;
			this.scaleY = 1;

		}
		public function reSize()
		{
		}
		protected function setRectangle():void
		{
			rectangle = new Rectangle();
			rectangle.x = this.scale9Grid.x;
			rectangle.y = this.scale9Grid.y;
			rectangle.width = this.scale9Grid.width;
			rectangle.height = this.scale9Grid.height;
		}
	}
}