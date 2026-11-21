// The scheme and weapon set editors (frontend-qt6 ui/page/pagescheme.cpp,
// ui/widget/selectWeapon.cpp). Every change is saved at once, to the files
// the Qt frontend uses.
#include "Editors.h"
#include "Game.h"
#include "SchemeData.h"

#include <Alert.h>
#include <Bitmap.h>
#include <Button.h>
#include <CheckBox.h>
#include <File.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <Spinner.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <algorithm>
#include <math.h>
#include <stdio.h>

enum { kMsgSelect = 'esel', kMsgNew = 'enew', kMsgCopy = 'ecpy', kMsgDelete = 'edel',
	kMsgRename = 'eren', kMsgEdit = 'edit', kMsgShow = 'eshw' };

static BPath frames_path() { return BPath(gUserDir.Path(), "editor_windows"); }

// The list of sets with New, Copy and Delete, and a name field; the form beside it
// is the subclass's. Predefined sets can be copied but not changed.
class EditorWindow : public BWindow {
public:
	EditorWindow(const char* title, const char* key, const char* noun, BMessenger launcher)
		: BWindow(BRect(120, 120, 760, 600), title, B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
		fKey(key), fNoun(noun), fLauncher(launcher)
	{
		fList = new BListView("list");
		fList->SetSelectionMessage(new BMessage(kMsgSelect));
		fListScroll = new BScrollView("list scroll", fList, 0, false, true);
		fListScroll->SetExplicitMinSize(BSize(be_plain_font->StringWidth("Balanced Random Weapon  (predefined)") + 30, 200));
		fNew = new BButton("New", new BMessage(kMsgNew));
		fCopy = new BButton("Copy", new BMessage(kMsgCopy));
		fDelete = new BButton("Delete", new BMessage(kMsgDelete));
		fName = new BTextControl("Name:", "", new BMessage(kMsgRename));
	}

	// called by the subclass once its form is built
	void Build(BView* form)
	{
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 0)
				.Add(fListScroll)
				.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
					.Add(fNew)
					.Add(fCopy)
					.Add(fDelete)
				.End()
			.End()
			.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING, 3)
				.Add(fName)
				.Add(form)
			.End();
		BFile f(frames_path().Path(), B_READ_ONLY);
		BMessage m;
		BRect frame;
		CenterOnScreen();
		if (m.Unflatten(&f) == B_OK && m.FindRect(fKey, &frame) == B_OK) {
			MoveTo(frame.LeftTop());
			ResizeTo(frame.Width(), frame.Height());
			MoveOnScreen();
		}
	}

	void Fill(const std::string& select)
	{
		while (BListItem* item = fList->RemoveItem((int32)0))
			delete item;
		int sel = 0;
		for (int i = 0; i < Count(); i++) {
			std::string label = NameAt(i);
			if (PredefinedAt(i))
				label += "  (predefined)";
			fList->AddItem(new BStringItem(label.c_str()));
			if (same_name(NameAt(i), select))
				sel = i;
		}
		fList->Select(sel);
		fList->ScrollToSelection();
	}

	void MessageReceived(BMessage* msg) override
	{
		int i = fList->CurrentSelection();
		switch (msg->what) {
			case kMsgSelect: {
				if (i < 0)
					break;
				bool editable = !PredefinedAt(i);
				fLoading = true;
				fName->SetText(NameAt(i).c_str());
				fName->SetEnabled(editable);
				fDelete->SetEnabled(editable);
				for (BControl* c : fControls)
					c->SetEnabled(editable);
				for (BMenuField* m : fMenus)
					m->SetEnabled(editable);
				ToForm(i);
				fLoading = false;
				break;
			}
			case kMsgNew:
			case kMsgCopy: {
				std::vector<std::string> names;
				for (int j = 0; j < Count(); j++)
					names.push_back(NameAt(j));
				std::string name = unique_name(msg->what == kMsgNew || i < 0 ? "New" : "Copy of " + NameAt(i), names);
				int at = Add(msg->what == kMsgNew ? -1 : i, name);
				Changed(at, name);
				Fill(name);
				fName->MakeFocus();
				break;
			}
			case kMsgDelete: {
				if (i < 0 || PredefinedAt(i))
					break;
				std::string text = "Delete the " + fNoun + " \"" + NameAt(i) + "\"?";
				if ((new BAlert(Title(), text.c_str(), "Cancel", "Delete", NULL, B_WIDTH_AS_USUAL,
						B_WARNING_ALERT))->Go() != 1)
					break;
				remove(Path(NameAt(i)).c_str());
				Remove(i);
				Changed(-1, "");
				Fill(NameAt(std::min(i, Count() - 1)));
				break;
			}
			case kMsgRename: {
				if (i < 0 || PredefinedAt(i) || NameAt(i) == fName->Text())
					break;
				std::string name = fName->Text(), problem;
				for (int j = 0; j < Count(); j++)
					if (j != i && same_name(NameAt(j), name))
						problem = "There is already a " + fNoun + " named \"" + NameAt(j) + "\". Choose another name.";
				if (!valid_name(name))
					problem = "A name can't be empty or contain ':', '/' or '\\'. Choose another name.";
				if (!problem.empty()) {
					fName->SetText(NameAt(i).c_str());
					(new BAlert(Title(), problem.c_str(), "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go(NULL);
					break;
				}
				std::string old = NameAt(i);
				SetName(i, name);
				if (Changed(i, name))
					remove(Path(old).c_str());
				((BStringItem*)fList->ItemAt(i))->SetText(name.c_str());
				fList->InvalidateItem(i);
				break;
			}
			case kMsgShow: {
				const char* select;
				if (msg->FindString("select", &select) == B_OK)
					Fill(select);
				Activate();
				break;
			}
			case kMsgEdit:
				if (!fLoading && i >= 0 && !PredefinedAt(i)) {
					FromForm(i);
					Changed(i, "");
				}
				break;
			default:
				BWindow::MessageReceived(msg);
		}
	}

	bool QuitRequested() override
	{
		// a name typed but not yet entered still counts
		if (fName->TextView()->IsFocus()) {
			BMessage rename(kMsgRename);
			MessageReceived(&rename);
		}
		BFile f(frames_path().Path(), B_READ_ONLY);
		BMessage m;
		m.Unflatten(&f);
		m.RemoveName(fKey);
		m.AddRect(fKey, Frame());
		save_message(m, frames_path());
		return true;
	}

protected:
	virtual int Count() = 0;
	virtual std::string NameAt(int i) = 0;
	virtual bool PredefinedAt(int i) = 0;
	virtual void SetName(int i, const std::string& name) = 0;
	virtual void ToForm(int i) = 0;
	virtual void FromForm(int i) = 0;
	virtual int Add(int copyOf, const std::string& name) = 0;	// returns its index
	virtual void Remove(int i) = 0;
	virtual bool Save(int i) = 0;
	virtual std::string Path(const std::string& name) = 0;

	// saves item i (if any) and tells the launcher
	bool Changed(int i, const std::string& select)
	{
		bool ok = i < 0 || Save(i);
		if (!ok) {
			std::string text = "Could not save the " + fNoun + ". Check that there is space on the disk.";
			(new BAlert(Title(), text.c_str(), "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go(NULL);
		}
		BMessage m(kMsgEditorsChanged);
		if (!select.empty())
			m.AddString("select", select.c_str());
		m.AddString("kind", fKey);
		fLauncher.SendMessage(&m);
		return ok;
	}

	std::vector<BControl*> fControls;	// the form's, disabled for predefined sets
	std::vector<BMenuField*> fMenus;
	bool fLoading = false;

private:
	const char* fKey;
	std::string fNoun;
	BMessenger fLauncher;
	BListView* fList;
	BScrollView* fListScroll;
	BButton *fNew, *fCopy, *fDelete;
	BTextControl* fName;
};

static BMenuField* popup(const char* label, const std::vector<std::string>& items)
{
	BPopUpMenu* menu = new BPopUpMenu("");
	for (const std::string& s : items)
		menu->AddItem(new BMenuItem(s.c_str(), new BMessage(kMsgEdit)));
	return new BMenuField(label, menu);
}

// a spinner's text and its two buttons, which grow with the font
static float spinner_width(const char* digits)
{
	font_height fh;
	be_plain_font->GetHeight(&fh);
	return be_plain_font->StringWidth(digits) + 2 * ceilf(fh.ascent + fh.descent + fh.leading) + 12;
}

static int marked(BMenuField* f) { return f->Menu()->IndexOf(f->Menu()->FindMarked()); }

// ── schemes ──
// pagescheme.cpp's toggles, in its order, with its "What's this" texts
static const struct { int index; const char* label; const char* tip; } kToggles[] = {
	{16, "Disable girders", "Disable girders when generating random maps."},
	{17, "Disable land objects", "Disable land objects when generating random maps."},
	{4, "Border", "Add an indestructible border around the terrain"},
	{25, "Bottom border", "Add an indestructible border along the bottom"},
	{1, "Switch hedgehog", "Select a hedgehog at the beginning of a turn"},
	{3, "Solid land", "Land can not be destroyed by most weapons."},
	{5, "Low gravity", "Lower gravity"},
	{22, "No wind", "You will not have to worry about wind anymore."},
	{23, "More wind", "Wind will affect almost everything."},
	{11, "Artillery", "Your hogs are unable to move, put your artillery skills to the test"},
	{9, "Vampirism", "Gain 80% of the damage you do back in health"},
	{10, "Karma", "Share your opponents pain, share their damage"},
	{8, "Reset health", "All (living) hedgehogs are fully restored at the end of turn"},
	{18, "AI survival", "AI respawns on death."},
	{7, "Invulnerable", "All hogs have a personal forcefield"},
	{21, "Per-hedgehog ammo", "Each hedgehog has its own ammo. It does not share with the team."},
	{15, "Shared ammo", "Ammo is shared between all teams that share a colour."},
	{20, "Reset weapons", "Weapons are reset to starting values each turn."},
	{19, "Unlimited attacks", "Attacking does not end your turn."},
	{6, "Laser sight", "Assisted aiming with laser sight"},
	{12, "Random order", "Order of play is random instead of in room order."},
	{14, "Place hedgehogs", "Take turns placing your hedgehogs before the start of play."},
	{2, "Divide teams", "Each clan starts in its own part of the terrain."},
	{24, "Tag team", "Teams in each clan take successive turns sharing their turn time."},
	{13, "King mode", "Play with a King. If he dies, your side dies."},
};

// the numbers; offset: shown value minus stored value (sudden death counts from 1)
static const struct { int index; const char* label; int min, max, offset; } kNumberFields[] = {
	{26, "Damage (%):", 10, 300, 0}, {27, "Turn time (s):", 1, 9999, 0},
	{28, "Initial health:", 1, 1000, 0}, {29, "Sudden death after (rounds):", 1, 52, 1},
	{39, "Water rise in sudden death:", 0, 100, 0}, {40, "Health loss in sudden death:", 0, 1000, 0},
	{41, "Rope length (%):", 25, 999, 0}, {37, "Health crates (%):", 0, 100, 0},
	{38, "Health in crates:", 0, 1000, 0}, {32, "Mines:", 0, 200, 0},
	{33, "Dud mines (%):", 0, 100, 0}, {34, "Barrels:", 0, 200, 0},
	{35, "Air mines:", 0, 200, 0}, {36, "Sentry bots:", 0, 200, 0},
	{42, "Retreat time (%):", 0, 999, 0},
};
const int kNumberCount = sizeof(kNumberFields) / sizeof(kNumberFields[0]);

class SchemeWindow : public EditorWindow {
public:
	SchemeWindow(BMessenger launcher)
		: EditorWindow("Schemes", "schemes", "scheme", launcher),
		fDir(BPath(gUserDir.Path(), "Schemes/Game").Path())
	{
		fSchemes = load_schemes(fDir);
		BGridView* modifiers = new BGridView("Game modifiers");
		int n = sizeof(kToggles) / sizeof(kToggles[0]);
		for (int t = 0; t < n; t++) {
			BCheckBox* c = fToggle[t] = new BCheckBox(kToggles[t].label, new BMessage(kMsgEdit));
			c->SetToolTip(kToggles[t].tip);
			modifiers->GridLayout()->AddView(c, t / ((n + 1) / 2), t % ((n + 1) / 2));
			fControls.push_back(c);
		}
		modifiers->GridLayout()->SetInsets(B_USE_DEFAULT_SPACING);
		modifiers->GridLayout()->SetSpacing(B_USE_DEFAULT_SPACING, 0);

		BGridView* settings = new BGridView("Basic settings");
		BGridLayout* grid = settings->GridLayout();
		grid->SetInsets(B_USE_DEFAULT_SPACING);
		grid->SetSpacing(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING);
		int row = 0;
		for (int f = 0; f < kNumberCount; f++, row++) {
			BSpinner* s = fNumber[f] = new BSpinner(kSchemeKeys[kNumberFields[f].index], kNumberFields[f].label,
				new BMessage(kMsgEdit));
			s->SetRange(kNumberFields[f].min, kNumberFields[f].max);
			int col = f < 8 ? 0 : 2;
			row = f < 8 ? f : f - 8;
			grid->AddItem(s->CreateLabelLayoutItem(), col, row);
			BLayoutItem* field = s->CreateTextViewLayoutItem();
			field->SetExplicitMinSize(BSize(spinner_width("99999"), B_SIZE_UNSET));
			grid->AddItem(field, col + 1, row);
			fControls.push_back(s);
		}
		std::vector<std::string> crates = {"Never", "Every turn"};
		for (int i = 2; i <= 9; i++)
			crates.push_back("Every " + std::to_string(i) + " turns");
		fCrates = popup("Crate drops:", crates);
		fMinesTime = popup("Mine timers:", {"Random", "0 s", "1 s", "2 s", "3 s", "4 s", "5 s"});
		fWorldEdge = popup("World edge:", {"None", "Wrap", "Bounce", "Sea"});
		fScriptParam = new BTextControl("Script parameter:", "", new BMessage(kMsgEdit));
		fScriptParam->SetToolTip("Additional parameter to configure game styles. The meaning depends on the used style.");
		BLayoutBuilder::Grid<>(grid)
			.AddMenuField(fCrates, 0, 8)
			.AddMenuField(fMinesTime, 2, 7)
			.AddMenuField(fWorldEdge, 2, 8)
			.AddTextControl(fScriptParam, 0, 9, B_ALIGN_HORIZONTAL_UNSET, 1, 3)
			.AddGlue(0, 10);
		fMenus = {fCrates, fMinesTime, fWorldEdge};
		fControls.push_back(fScriptParam);

		BTabView* tabs = new BTabView("tabs", B_WIDTH_FROM_LABEL);
		tabs->AddTab(settings);
		tabs->AddTab(modifiers);
		Build(tabs);
	}

protected:
	int Count() override { return fSchemes.size(); }
	std::string NameAt(int i) override { return i >= 0 && i < Count() ? fSchemes[i].name : ""; }
	bool PredefinedAt(int i) override { return fSchemes[i].predefined; }
	void SetName(int i, const std::string& name) override { fSchemes[i].name = name; }
	std::string Path(const std::string& name) override { return fDir + "/" + name + ".hwg"; }
	bool Save(int i) override { return save_scheme(fDir, fSchemes[i]); }
	void Remove(int i) override { fSchemes.erase(fSchemes.begin() + i); }

	int Add(int copyOf, const std::string& name) override
	{
		GameScheme s = copyOf >= 0 ? fSchemes[copyOf] : predefined_schemes()[0];
		s.name = name;
		s.predefined = false;
		fSchemes.push_back(s);
		return Count() - 1;
	}

	void ToForm(int i) override
	{
		const GameScheme& s = fSchemes[i];
		for (size_t t = 0; t < sizeof(kToggles) / sizeof(kToggles[0]); t++)
			fToggle[t]->SetValue(s.v[kToggles[t].index]);
		for (int f = 0; f < kNumberCount; f++)
			fNumber[f]->SetValue(s.v[kNumberFields[f].index] + kNumberFields[f].offset);
		Mark(fCrates, s.v[30]);
		Mark(fMinesTime, s.v[31] + 1);
		Mark(fWorldEdge, s.v[43]);
		fScriptParam->SetText(s.scriptparam.c_str());
	}

	void FromForm(int i) override
	{
		GameScheme& s = fSchemes[i];
		for (size_t t = 0; t < sizeof(kToggles) / sizeof(kToggles[0]); t++)
			s.v[kToggles[t].index] = fToggle[t]->Value() ? 1 : 0;
		for (int f = 0; f < kNumberCount; f++)
			s.v[kNumberFields[f].index] = fNumber[f]->Value() - kNumberFields[f].offset;
		s.v[30] = std::max(marked(fCrates), 0);
		s.v[31] = marked(fMinesTime) - 1;
		s.v[43] = std::max(marked(fWorldEdge), 0);
		s.scriptparam = fScriptParam->Text();
	}

private:
	static void Mark(BMenuField* f, int i)
	{
		if (BMenuItem* item = f->Menu()->ItemAt(std::max(i, 0)))
			item->SetMarked(true);
	}

	std::string fDir;
	std::vector<GameScheme> fSchemes;
	BCheckBox* fToggle[25];
	BSpinner* fNumber[kNumberCount];
	BMenuField *fCrates, *fMinesTime, *fWorldEdge;
	BTextControl* fScriptParam;
};

// ── weapon sets ──
// a 32x32 icon from Graphics/AmmoMenu/Ammos_base.png (4 columns of 16), as getAmmoImage cuts it
class AmmoIcon : public BView {
public:
	AmmoIcon(BBitmap* sheet, int index) : BView("icon", B_WILL_DRAW), fSheet(sheet), fIndex(index)
	{
		SetExplicitSize(BSize(31, 31));
	}
	void Draw(BRect) override
	{
		if (fSheet)
			DrawBitmap(fSheet, BRect(0, 0, 31, 31).OffsetByCopy(fIndex / 16 * 32, fIndex % 16 * 32), Bounds());
	}
private:
	BBitmap* fSheet;
	int fIndex;
};

// a view in a scroll view that sets the scroll bar from its layout's height
class ScrollGrid : public BGridView {
public:
	ScrollGrid() : BGridView("weapons") {}
	void FrameResized(float w, float h) override
	{
		BGridView::FrameResized(w, h);
		if (BScrollBar* bar = ScrollBar(B_VERTICAL)) {
			float total = PreferredSize().height;
			bar->SetRange(0, std::max(0.0f, total - h));
			bar->SetProportion(total > 0 ? std::min(1.0f, h / total) : 1);
			bar->SetSteps(32, h);
		}
	}
};

static const char* const kWeaponColumns[] = {"Count", "Probability", "Delay", "In crates"};
static const int kWeaponMax[] = {9, 8, 8, 8};	// SelWeaponWidget::readWeaponValue

class WeaponWindow : public EditorWindow {
public:
	WeaponWindow(BMessenger launcher)
		: EditorWindow("Weapon sets", "weapons", "weapon set", launcher),
		fDir(BPath(gUserDir.Path(), "Schemes/Ammo").Path())
	{
		fWeapons = load_weapons(fDir);
		fSheet = BTranslationUtils::GetBitmap(BPath(gDataDir.Path(), "Graphics/AmmoMenu/Ammos_base.png").Path());
		ScrollGrid* table = new ScrollGrid;
		BGridLayout* grid = table->GridLayout();
		grid->SetInsets(B_USE_SMALL_SPACING);
		grid->SetSpacing(B_USE_DEFAULT_SPACING, 2);
		for (int c = 0; c < 4; c++)
			grid->AddView(new BStringView("column", kWeaponColumns[c]), 2 + c, 0);
		int row = 1;
		for (int m = 0; m < kAmmoCount; m++) {
			int a = kAmmoMenu[m] - 1;
			if (a == 6 || a == 57)	// Skip and the unused Creeper, hidden as in the Qt frontend
				continue;
			grid->AddView(new AmmoIcon(fSheet, a), 0, row);
			grid->AddView(new BStringView("name", kAmmoNames[a]), 1, row);
			for (int c = 0; c < 4; c++) {
				BSpinner* s = new BSpinner("value", NULL, new BMessage(kMsgEdit));
				s->SetRange(0, kWeaponMax[c]);
				s->SetExplicitMaxSize(BSize(spinner_width("99"), B_SIZE_UNSET));
				grid->AddView(s, 2 + c, row);
				fSpin.push_back({s, a, c});
				fControls.push_back(s);
			}
			row++;
		}
		BScrollView* scroll = new BScrollView("weapons scroll", table, 0, false, true);
		scroll->SetExplicitMinSize(BSize(table->PreferredSize().width + B_V_SCROLL_BAR_WIDTH + 4, 300));
		BStringView* hint = new BStringView("hint", "A count of 9 is unlimited; delays are in turns.");
		BGroupView* form = new BGroupView(B_VERTICAL, B_USE_SMALL_SPACING);
		BLayoutBuilder::Group<>(form).Add(scroll).Add(hint);
		Build(form);
	}
	~WeaponWindow() { delete fSheet; }

protected:
	int Count() override { return fWeapons.size(); }
	std::string NameAt(int i) override { return i >= 0 && i < Count() ? fWeapons[i].name : ""; }
	bool PredefinedAt(int i) override { return fWeapons[i].predefined; }
	void SetName(int i, const std::string& name) override { fWeapons[i].name = name; }
	std::string Path(const std::string& name) override { return fDir + "/" + name + ".hwa"; }
	bool Save(int i) override { return save_weapons(fDir, fWeapons[i]); }
	void Remove(int i) override { fWeapons.erase(fWeapons.begin() + i); }

	int Add(int copyOf, const std::string& name) override
	{
		fWeapons.push_back({name, copyOf >= 0 ? fWeapons[copyOf].ammo : kEmptyAmmo, false});
		return Count() - 1;
	}

	void ToForm(int i) override
	{
		const std::string& ammo = fWeapons[i].ammo;
		for (const Spin& s : fSpin) {
			char ch = ammo[s.column * kAmmoCount + s.ammo];
			s.view->SetValue(ch >= '0' && ch <= '9' ? std::min(ch - '0', kWeaponMax[s.column]) : 0);
		}
	}

	void FromForm(int i) override
	{
		// Skip and Creeper keep what the set had
		for (const Spin& s : fSpin)
			fWeapons[i].ammo[s.column * kAmmoCount + s.ammo] = '0' + s.view->Value();
	}

private:
	struct Spin { BSpinner* view; int ammo, column; };
	std::string fDir;
	std::vector<WeaponSet> fWeapons;
	std::vector<Spin> fSpin;
	BBitmap* fSheet;
};

// one window of each kind; showing it again brings it to the front
template<class W> static void show_editor(BMessenger& open, BMessenger launcher, const std::string& select)
{
	BMessage show(kMsgShow);
	show.AddString("select", select.c_str());
	if (open.IsValid() && open.SendMessage(&show) == B_OK)
		return;
	W* w = new W(launcher);
	w->Fill(select);
	open = BMessenger(w);
	w->Show();
}

static BMessenger sSchemes, sWeapons;

void show_scheme_editor(BMessenger launcher, const std::string& select)
{
	show_editor<SchemeWindow>(sSchemes, launcher, select);
}

void show_weapon_editor(BMessenger launcher, const std::string& select)
{
	show_editor<WeaponWindow>(sWeapons, launcher, select);
}
