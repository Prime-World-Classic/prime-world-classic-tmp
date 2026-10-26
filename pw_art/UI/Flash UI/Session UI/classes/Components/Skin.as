package Components
{
	import flash.display.Bitmap;
	import flash.display.BitmapData;
	import flash.display.DisplayObject;
	import flash.display.DisplayObjectContainer;
	import flash.display.Sprite;
	import flash.geom.Rectangle;
	import flash.filters.ColorMatrixFilter;
	import flash.utils.Dictionary;
	import flash.utils.getDefinitionByName;

	// Halloween skin of the windows and controls (chosen in the castle).
	// Bitmaps HW_* are in the library of main.fla (folder "halloween", art/halloween/prep_halloween.py),
	// stored 2x of the size on screen. An element without its bitmap keeps the usual look.
	public class Skin
	{
		// TODO: set from C++ (castle choice); for now the halloween skin is the default
		public static var halloween:Boolean = false;

		private static var cache:Object = {};
		// errors of the skin ("where:errorID; ..."), for debugging
		public static var lastError:String = "";

		// a new BitmapData of the library bitmap: in the game's player a BitmapData belongs to one Bitmap only
		// (giving it to another Bitmap takes it from the first one), so it is never shared
		public static function Bmd(name:String):BitmapData
		{
			if (!halloween || name == null)
				return null;
			if (!Has(name))
				return null;
			var c:Class = cache[name] as Class;
			// as the other library bitmaps (ImpulsTalentIco): "new c" without arguments
			return new c;
		}

		// an error of the skin must not break the window: the element stays as it is, the error is remembered
		public static function Fail(where:String, e:Error):void
		{
			var s:String = where + ":" + e.errorID;
			if (lastError.indexOf(s) < 0)
				lastError = (lastError == "" ? "" : lastError + "; ") + s;
		}

		// is there such a bitmap in the library (without creating it)
		public static function Has(name:String):Boolean
		{
			if (!halloween || name == null)
				return false;
			if (cache[name] === undefined)
			{
				try
				{
					cache[name] = getDefinitionByName(name) as Class;
				}
				catch (e:Error)
				{
					cache[name] = null;
				}
			}
			return cache[name] != null;
		}

		// Recolouring of the teal HUD parts (chat buttons and input, action bar slots, lock, minimap buttons, day/night,
		// level back): teal -> purple, yellow -> orange, gold and white stay. Rows sum to 1 (white stays white);
		// fitted on teal (20,90,95)->(70,35,90) and gold (200,170,90)->(200,150,80).
		private static var tintFilter:ColorMatrixFilter = new ColorMatrixFilter([
			0.267, 1.008, -0.275, 0, 0,
			0.826, -0.385, 0.559, 0, 0,
			0.081, -0.237, 1.156, 0, 0,
			0, 0, 0, 1, 0]);
		private static var tinted:Dictionary = new Dictionary(true);

		public static function Tint(d:DisplayObject):void
		{
			if (d == null || !Active)
				return;
			tinted[d] = true;
			d.filters = [tintFilter];
		}

		// Tint for every child of c but the given ones (icons)
		public static function TintChildren(c:DisplayObjectContainer, except:Array = null):void
		{
			if (c == null || !Active)
				return;
			for (var i:int = 0; i < c.numChildren; i++)
			{
				var d:DisplayObject = c.getChildAt(i);
				if (except == null || except.indexOf(d) < 0)
					Tint(d);
			}
		}

		// filters of a tinted object without the desaturation (src.Desaturate.saturate)
		public static function NormalFilters(d:DisplayObject):Array
		{
			return (d != null && tinted[d]) ? [tintFilter] : null;
		}

		// text of a selected element (category, slot, wheel sector): orange instead of the usual (white) one
		public static const SELECTED_TEXT:uint = 0xFFA040;

		public static function SelectedColor(usual:uint):uint
		{
			return Active ? SELECTED_TEXT : usual;
		}

		// textures of the chat / ping wheel: "chat_wheel_" -> "chat_wheel_hw_" (pw_art/UI/PingWheel/wheel_hd.py)
		public static function WheelPrefix(prefix:String):String
		{
			return Active ? prefix + "hw_" : prefix;
		}

		public static function get Active():Boolean
		{
			return Has("HW_Wood");
		}

		// Redrawing a container (window frame, slider track) reuses its bitmaps: a new BitmapData is a new texture
		// of the engine (the wood is 1024x1024), and windows are resized often
		private static var pools:Dictionary = new Dictionary(true);
		private static var pool:Object = null;
		private static var used:Object = null;

		// removes the children of s; until EndReuse the bitmaps of Bmp come from the previous drawing of s
		public static function BeginReuse(s:DisplayObjectContainer):void
		{
			while (s.numChildren > 0)
				s.removeChildAt(0);
			if (pools[s] == null)
				pools[s] = {};
			pool = pools[s];
			used = {};
		}

		public static function EndReuse():void
		{
			pool = null;
			used = null;
		}

		// bitmap of natural size (half of the stored one) or of the given size
		public static function Bmp(name:String, w:Number = NaN, h:Number = NaN):Bitmap
		{
			var b:Bitmap = null;
			if (pool != null)
			{
				if (pool[name] == null)
					pool[name] = [];
				var list:Array = pool[name];
				var n:int = int(used[name]);
				used[name] = n + 1;
				if (n < list.length)
				{
					b = Bitmap(list[n]);
					b.scaleX = b.scaleY = 1;
					b.visible = true;
				}
			}
			if (b == null)
			{
				var bd:BitmapData = Bmd(name);
				if (bd == null)
					return null;
				b = new Bitmap(bd, "auto", true);
				if (pool != null)
					pool[name].push(b);
			}
			b.width = isNaN(w) ? b.bitmapData.width / 2 : w;
			b.height = isNaN(h) ? b.bitmapData.height / 2 : h;
			return b;
		}

		public static function Put(parent:DisplayObjectContainer, name:String, x:Number, y:Number, w:Number = NaN, h:Number = NaN):Bitmap
		{
			var b:Bitmap = Bmp(name, w, h);
			if (b == null)
				return null;
			b.x = x;
			b.y = y;
			parent.addChild(b);
			return b;
		}

		// hides the content of a state clip and puts the bitmap (natural size * k) at the centre of the old content
		public static function Fill(mc:DisplayObjectContainer, name:String, k:Number = 1):Bitmap
		{
			if (mc == null || !Has(name))
				return null;
			// bounds of the content (not getBounds: the game's player returns null for it on timeline bitmaps)
			var r:Rectangle = null;
			for (var i:int = 0; i < mc.numChildren; i++)
			{
				var c:DisplayObject = mc.getChildAt(i);
				var cr:Rectangle = new Rectangle(c.x, c.y, c.width, c.height);
				r = (r == null) ? cr : r.union(cr);
				c.visible = false;
			}
			if (r == null)
				r = new Rectangle(0, 0, 0, 0);
			var b:Bitmap = Bmp(name);
			b.width *= k;
			b.height *= k;
			b.x = r.x + (r.width - b.width) / 2;
			b.y = r.y + (r.height - b.height) / 2;
			mc.addChild(b);
			return b;
		}

		// horizontal 3-slice: left cap, stretched middle, right cap; height h (caps keep their aspect)
		public static function HBar(parent:DisplayObjectContainer, prefix:String, x:Number, y:Number, w:Number, h:Number):void
		{
			var l:Bitmap = Bmp(prefix + "L");
			var m:Bitmap = Bmp(prefix + "M");
			var r:Bitmap = Bmp(prefix + "R");
			if (l == null || m == null || r == null)
				return;
			var k:Number = h / l.height;
			l.width *= k; l.height = h;
			r.width *= k; r.height = h;
			if (l.width + r.width > w)
			{
				l.width = r.width = w / 2;
			}
			l.x = x; l.y = y;
			r.x = x + w - r.width; r.y = y;
			m.x = l.x + l.width - 0.5; m.y = y;
			m.width = Math.max(0, r.x - m.x + 1); m.height = h;
			parent.addChild(m);
			parent.addChild(l);
			parent.addChild(r);
		}

		// window: wood, title plate, cobwebs, frame around (outside of 0..w x 0..h by 10 px) and a bat on top
		// plain = semi-transparent purple instead of the wood (windows with many icons: talents, statistics)
		public static function DrawWindow(s:Sprite, w:Number, h:Number, plain:Boolean = false):void
		{
			BeginReuse(s);
			try
			{
				DrawWindowParts(s, w, h, plain);
			}
			finally
			{
				EndReuse();
			}
		}

		private static function DrawWindowParts(s:Sprite, w:Number, h:Number, plain:Boolean):void
		{
			if (w < 20 || h < 20)
				return;

			if (plain)
				Put(s, "HW_Plain", 0, 0, w, h);		// a bitmap: a Shape here is not drawn by the game's player
			else
				Put(s, "HW_Wood", 0, 0, w, h);
			if (h > 60)
				HBar(s, "HW_Header", 3, 1, w - 6, 28);

			var ws:Number = Math.min(64, w / 4, h / 3);
			if (ws > 16)
			{
				Mirrored(s, "HW_Web", w - 4, 30, -ws);			// top right, under the title plate
				Mirrored(s, "HW_Web", 4, h - 4, ws, true);		// bottom left
				Mirrored(s, "HW_Web", w - 4, h - 4, -ws, true);	// bottom right
			}

			var O:Number = 10, t:Number = 14;
			var x0:Number = -O, y0:Number = -O, x1:Number = w + O, y1:Number = h + O;
			var tl:Bitmap = Bmp("HW_FrameTL");
			if (tl == null)
				return;
			var c:Number = tl.width;
			Edge(s, "HW_FrameT", "HW_FrameOT", x0 + c, y0, x1 - x0 - 2 * c, t, true);
			Edge(s, "HW_FrameB", "HW_FrameOB", x0 + c, y1 - t, x1 - x0 - 2 * c, t, true);
			Edge(s, "HW_FrameL", "HW_FrameOL", x0, y0 + c, y1 - y0 - 2 * c, t, false);
			Edge(s, "HW_FrameR", "HW_FrameOR", x1 - t, y0 + c, y1 - y0 - 2 * c, t, false);
			Put(s, "HW_FrameTL", x0, y0);
			Put(s, "HW_FrameTR", x1 - c, y0);
			Put(s, "HW_FrameBL", x0, y1 - c);
			Put(s, "HW_FrameBR", x1 - c, y1 - c);

			if (w > 200)
			{
				var bat:Bitmap = Bmp("HW_Bat");
				bat.x = (w - bat.width) / 2;
				bat.y = y0 - bat.height * 0.55;
				s.addChild(bat);
			}
		}

		// bitmap of width |w| (aspect kept) at x,y; negative w mirrors it to the left, flipY mirrors it upwards
		private static function Mirrored(s:Sprite, name:String, x:Number, y:Number, w:Number, flipY:Boolean = false):void
		{
			var b:Bitmap = Bmp(name);
			if (b == null)
				return;
			var k:Number = Math.abs(w) / b.bitmapData.width;
			b.scaleX = w < 0 ? -k : k;
			b.scaleY = flipY ? -k : k;
			b.x = x;
			b.y = y;
			s.addChild(b);
		}

		// side of the frame from 'pos' of length 'len': stretched plain strip with the ornament in the middle
		private static function Edge(s:Sprite, plain:String, orn:String, px:Number, py:Number, len:Number, t:Number, horizontal:Boolean):void
		{
			if (len <= 0)
				return;
			var o:Bitmap = Bmp(orn);
			var olen:Number = (o == null) ? 0 : (horizontal ? o.width : o.height);
			if (olen > len - 8)
			{
				o = null;
				olen = 0;
			}
			var a:Number = (len - olen) / 2;	// length of each plain part
			for (var i:int = 0; i < 2; i++)
			{
				var p:Bitmap = Bmp(plain);
				if (p == null)
					return;
				var start:Number = (i == 0) ? 0 : a + olen - 0.5;
				if (horizontal)
				{
					p.x = px + start; p.y = py; p.width = a + 1; p.height = t;
				}
				else
				{
					p.x = px; p.y = py + start; p.width = t; p.height = a + 1;
				}
				s.addChild(p);
			}
			if (o != null)
			{
				if (horizontal) { o.x = px + a; o.y = py; }
				else { o.x = px; o.y = py + a; }
				s.addChild(o);
			}
		}
	}
}
