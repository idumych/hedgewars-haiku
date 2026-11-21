// The hand-drawn map window (see DrawMap.h)
#include "DrawMap.h"
#include "DrawnMap.h"
#include "Game.h"

#include <Alert.h>
#include <Bitmap.h>
#include <Button.h>
#include <CheckBox.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <GradientLinear.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <RadioButton.h>
#include <Region.h>
#include <Slider.h>
#include <StringView.h>
#include <Window.h>

#include <algorithm>
#include <stdio.h>

enum {
	kMsgPencil = 'dpen', kMsgRectangle = 'drec', kMsgEllipse = 'dell', kMsgEraser = 'dera',
	kMsgWidth = 'dwid', kMsgUndo = 'dund', kMsgClear = 'dclr', kMsgOpen = 'dopn', kMsgSave = 'dsav',
	kMsgDrawingChanged = 'dchg',
};

static BPath current_path() { return BPath(gUserDir.Path(), "drawn.hwmap"); }

static std::string read_file(const char* path)
{
	BFile f(path, B_READ_ONLY);
	off_t size = 0;
	std::string s;
	if (f.GetSize(&size) == B_OK && size > 0 && size < (16 << 20)) {
		s.resize(size);
		if (f.Read(&s[0], size) != size)
			s.clear();
	}
	return s;
}

static bool write_file(const char* path, const std::string& s)
{
	BFile f(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	return f.InitCheck() == B_OK && f.Write(s.data(), s.size()) == (ssize_t)s.size();
}

static std::string current_data()
{
	std::string data;
	return from_hwmap(read_file(current_path().Path()), data) ? data : "";
}

std::string drawn_map_file() { return read_file(current_path().Path()); }

bool drawn_map_empty() { return decode_drawing(current_data()).strokes.empty(); }

std::string drawn_map_config()
{
	std::string data = current_data(), buf;
	for (size_t i = 0; i < data.size(); i += 200)	// 40 points a message
		add(buf, "edraw " + data.substr(i, 200));
	return buf;
}

// the map, scaled to fit; draws with the mouse like DrawMapScene
class Canvas : public BView {
public:
	Canvas() : BView("canvas", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_FRAME_EVENTS)
	{
		SetExplicitMinSize(BSize(512, 256));
		SetViewColor(B_TRANSPARENT_COLOR);
		fDrawing = decode_drawing(current_data());
	}
	~Canvas() { delete fBuffer; }

	Drawing fDrawing;
	std::vector<Drawing> fUndo;
	int fWidth = kPenStart;
	bool fErasing = false;
	uint32 fTool = kMsgPencil;

	void Changed()
	{
		write_file(current_path().Path(), to_hwmap(encode_drawing(fDrawing)));
		Invalidate();
	}

	void Edit()	// before every change, for Undo
	{
		fUndo.push_back(fDrawing);
		if (fUndo.size() > 200)
			fUndo.erase(fUndo.begin());
	}

	void FrameResized(float, float) override
	{
		delete fBuffer;
		fBuffer = NULL;
	}

	BRect MapRect()
	{
		BRect b = Bounds();
		float scale = std::min((b.Width() + 1) / kMapWidth, (b.Height() + 1) / kMapHeight);
		BRect r(0, 0, kMapWidth * scale - 1, kMapHeight * scale - 1);
		r.OffsetBy((b.Width() - r.Width()) / 2, (b.Height() - r.Height()) / 2);
		return r;
	}

	BPoint ToView(int x, int y)
	{
		BRect r = MapRect();
		return BPoint(r.left + x * (r.Width() + 1) / kMapWidth, r.top + y * (r.Height() + 1) / kMapHeight);
	}

	std::pair<int, int> ToMap(BPoint p)
	{
		BRect r = MapRect();
		return {(int)((p.x - r.left) * kMapWidth / (r.Width() + 1)), (int)((p.y - r.top) * kMapHeight / (r.Height() + 1))};
	}

	void Draw(BRect) override
	{
		BRect b = Bounds();
		if (!fBuffer) {
			fBuffer = new BBitmap(b, B_RGB32, true);
			fBuffer->AddChild(new BView(b, "buffer", 0, 0));
		}
		if (!fBuffer->Lock())
			return;
		BView* v = fBuffer->ChildAt(0);
		BRect r = MapRect();
		v->SetHighUIColor(B_PANEL_BACKGROUND_COLOR);
		v->FillRect(b);
		// DrawMapScene's colours: land yellow on a blue-to-olive gradient
		BGradientLinear sky(r.LeftTop(), r.LeftBottom());
		sky.AddColor(make_color(60, 60, 155), 0);
		sky.AddColor(make_color(155, 155, 60), 255);
		v->FillRect(r, sky);
		v->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
		float scale = (r.Width() + 1) / kMapWidth;
		v->ConstrainClippingRegion(new BRegion(r));
		for (const Stroke& s : fDrawing.strokes) {
			v->SetHighColor(255, 255, 0);
			v->SetPenSize(std::max(1.0f, s.width * scale));
			for (size_t i = 0; i < s.points.size(); i++) {
				BPoint p = ToView(s.points[i].first, s.points[i].second);
				BPoint q = i ? ToView(s.points[i - 1].first, s.points[i - 1].second) : p;
				if (s.erasing)
					v->StrokeLine(q, p, sky);
				else
					v->StrokeLine(q, p);
			}
		}
		v->ConstrainClippingRegion(NULL);
		v->Sync();
		fBuffer->Unlock();
		DrawBitmap(fBuffer, B_ORIGIN);
	}

	void MouseDown(BPoint where) override
	{
		MakeFocus();
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		Edit();
		Stroke s;
		s.width = fWidth;
		s.erasing = fErasing;
		s.points.push_back(fStart = ToMap(where));
		fDrawing.strokes.push_back(s);
		fDrawingNow = true;
		Invalidate();
	}

	void MouseMoved(BPoint where, uint32, const BMessage*) override
	{
		if (!fDrawingNow)
			return;
		std::pair<int, int> p = ToMap(where);
		auto& pts = fDrawing.strokes.back().points;
		if (fTool == kMsgPencil)
			pts.push_back(p);
		else if (fTool == kMsgRectangle)
			pts = rectangle_points(fStart.first, fStart.second, p.first, p.second);
		else
			pts = ellipse_points(fStart.first, fStart.second, p.first, p.second);
		Invalidate();
	}

	void MouseUp(BPoint where) override
	{
		if (!fDrawingNow)
			return;
		MouseMoved(where, 0, NULL);
		fDrawingNow = false;
		if (fTool == kMsgPencil)
			simplify(fDrawing.strokes.back());
		Changed();
	}

	void MessageReceived(BMessage* msg) override
	{
		if (msg->what == B_MOUSE_WHEEL_CHANGED) {	// the pen's width, as in the Qt frontend
			float dy = 0;
			msg->FindFloat("be:wheel_delta_y", &dy);
			fWidth = std::min(std::max(fWidth + (dy < 0 ? 10 : -10), (int)kPenMin), (int)kPenMax);
			Window()->PostMessage(kMsgDrawingChanged);
			return;
		}
		BView::MessageReceived(msg);
	}

private:
	BBitmap* fBuffer = NULL;
	bool fDrawingNow = false;
	std::pair<int, int> fStart;
};

static BWindow* sDrawWindow;

class DrawMapWindow : public BWindow {
public:
	DrawMapWindow()
		: BWindow(BRect(0, 0, 900, 500), "Hand-drawn map", B_DOCUMENT_WINDOW,
			B_AUTO_UPDATE_SIZE_LIMITS)
	{
		BMenuBar* bar = new BMenuBar("menu");
		BMenu* file = new BMenu("File");
		file->AddItem(new BMenuItem("Open…", new BMessage(kMsgOpen), 'O'));
		file->AddItem(new BMenuItem("Save as…", new BMessage(kMsgSave), 'S', B_SHIFT_KEY));
		file->AddSeparatorItem();
		file->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED), 'W'));
		bar->AddItem(file);
		BMenu* edit = new BMenu("Edit");
		edit->AddItem(fUndoItem = new BMenuItem("Undo", new BMessage(kMsgUndo), 'Z'));
		edit->AddItem(new BMenuItem("Clear", new BMessage(kMsgClear)));
		bar->AddItem(edit);

		fCanvas = new Canvas;
		BRadioButton* pencil = new BRadioButton("Pencil", new BMessage(kMsgPencil));
		pencil->SetValue(B_CONTROL_ON);
		fEraser = new BCheckBox("Eraser", new BMessage(kMsgEraser));
		fWidth = new BSlider("width", "Line width:", new BMessage(kMsgWidth), kPenMin, kPenMax, B_HORIZONTAL);
		fWidth->SetModificationMessage(new BMessage(kMsgWidth));
		fWidth->SetKeyIncrementValue(10);
		fWidth->SetValue(kPenStart);
		fWidth->SetLimitLabels("Thin", "Thick");
		fWidth->SetExplicitMinSize(BSize(be_plain_font->StringWidth("Line width:") * 2, B_SIZE_UNSET));
		BStringView* hint = new BStringView("hint", "Yellow is land. Hedgehogs are placed on it.");
		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.Add(bar)
			.AddGroup(B_HORIZONTAL)
				.SetInsets(B_USE_WINDOW_SPACING)
				.AddGroup(B_VERTICAL)
					.Add(pencil)
					.Add(new BRadioButton("Rectangle", new BMessage(kMsgRectangle)))
					.Add(new BRadioButton("Ellipse", new BMessage(kMsgEllipse)))
					.AddStrut(B_USE_SMALL_SPACING)
					.Add(fEraser)
					.Add(fWidth)
					.AddStrut(B_USE_SMALL_SPACING)
					.Add(new BButton("Undo", new BMessage(kMsgUndo)))
					.Add(new BButton("Clear", new BMessage(kMsgClear)))
					.AddGlue()
				.End()
				.AddGroup(B_VERTICAL)
					.Add(fCanvas, 10)
					.Add(hint)
				.End()
			.End();
		CenterOnScreen();
		UpdateUndo();
	}

	~DrawMapWindow() { delete fPanel; }

	void UpdateUndo() { fUndoItem->SetEnabled(!fCanvas->fUndo.empty()); }

	void Panel(file_panel_mode mode)
	{
		delete fPanel;
		BPath dir(gUserDir.Path(), "DrawnMaps");
		create_directory(dir.Path(), 0755);
		entry_ref ref;
		get_ref_for_path(dir.Path(), &ref);
		fPanel = new BFilePanel(mode, new BMessenger(this), &ref, 0, false);
		fPanel->Window()->SetTitle(mode == B_OPEN_PANEL ? "Open" : "Save as");
		if (mode == B_SAVE_PANEL)
			fPanel->SetSaveText("map.hwmap");
		fPanel->Show();
	}

	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kMsgPencil: case kMsgRectangle: case kMsgEllipse:
				fCanvas->fTool = msg->what;
				break;
			case kMsgEraser: fCanvas->fErasing = fEraser->Value(); break;
			case kMsgWidth: fCanvas->fWidth = (fWidth->Value() - 6) / 10 * 10 + 6; break;
			case kMsgDrawingChanged: fWidth->SetValue(fCanvas->fWidth); break;
			case kMsgUndo:
				if (!fCanvas->fUndo.empty()) {
					fCanvas->fDrawing = fCanvas->fUndo.back();
					fCanvas->fUndo.pop_back();
					fCanvas->Changed();
				}
				break;
			case kMsgClear:
				if (!fCanvas->fDrawing.strokes.empty()) {
					fCanvas->Edit();
					fCanvas->fDrawing = Drawing();
					fCanvas->Changed();
				}
				break;
			case kMsgOpen: Panel(B_OPEN_PANEL); break;
			case kMsgSave: Panel(B_SAVE_PANEL); break;
			case B_REFS_RECEIVED: {
				entry_ref ref;
				std::string data;
				if (msg->FindRef("refs", &ref) != B_OK)
					break;
				if (!from_hwmap(read_file(BPath(&ref).Path()), data)) {
					(new BAlert("Hedgewars", "This file is not a drawn map. Open a .hwmap file "
						"saved by Hedgewars.", "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go(NULL);
					break;
				}
				fCanvas->Edit();
				fCanvas->fDrawing = decode_drawing(data);
				fCanvas->Changed();
				break;
			}
			case B_SAVE_REQUESTED: {
				entry_ref dir;
				const char* name;
				if (msg->FindRef("directory", &dir) != B_OK || msg->FindString("name", &name) != B_OK)
					break;
				BPath path(&dir);
				path.Append(name);
				if (!write_file(path.Path(), to_hwmap(encode_drawing(fCanvas->fDrawing))))
					(new BAlert("Hedgewars", "Could not save the map. Check that the disk isn't full "
						"or read-only.", "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go(NULL);
				break;
			}
			default:
				BWindow::MessageReceived(msg);
		}
		UpdateUndo();
	}

	bool QuitRequested() override
	{
		sDrawWindow = NULL;
		return true;
	}

private:
	Canvas* fCanvas;
	BCheckBox* fEraser;
	BSlider* fWidth;
	BMenuItem* fUndoItem;
	BFilePanel* fPanel = NULL;
};

void show_draw_map()
{
	if (!sDrawWindow)
		sDrawWindow = new DrawMapWindow;
	if (sDrawWindow->Lock()) {
		if (sDrawWindow->IsHidden())
			sDrawWindow->Show();
		sDrawWindow->Activate();
		sDrawWindow->Unlock();
	}
}
