package EscMenu
{
	import Components.Skin;
	import flash.display.Bitmap;
	import flash.display.DisplayObject;
	import flash.geom.Rectangle;
	import flash.text.TextField;
	import flash.display.MovieClip;

	public class ShortcutsHeader extends MovieClip
	{
		public var title:TextField;

		public function ShortcutsHeader()
		{
			ApplySkin();
		}

		// halloween skin: branch instead of the line, dark band behind the title, orange title
		private function ApplySkin():void
		{
			try
			{
				if (!Skin.Active)
					return;
				for (var i:int = numChildren - 1; i >= 0; i--)
				{
					var old:DisplayObject = getChildAt(i);
					// timeline bitmaps are not always Bitmap objects in the game's player: everything but the title
					if (old is TextField || !old.visible)
						continue;
					var r:Rectangle = new Rectangle(old.x, old.y, old.width, old.height);
					var line:Boolean = r.height < 8;
					var b:Bitmap = line ? Skin.Bmp("HW_Branch", r.width, 12) : Skin.Bmp("HW_HeaderBand", r.width, r.height);
					if (b == null)
						continue;
					b.x = r.x;
					b.y = line ? r.y + (r.height - 12) / 2 : r.y;
					old.visible = false;
					addChildAt(b, i + 1);
				}
				if (title != null)
					title.textColor = 0xFF9A3C;
			}
			catch (e:Error)
			{
				Skin.Fail("ShortcutsHeader", e);
			}
		}
	}
}
