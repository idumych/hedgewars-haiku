// The key binding list (frontend-qt6's KeyBinder): select an action, press
// "Set key" (or double-click it), then press the key or mouse button.
#include "Settings.h"

#include <Button.h>
#include <Font.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <ScrollView.h>
#include <Window.h>

#include <algorithm>

enum { kMsgSet = 'bset', kMsgDefault = 'bdef', kMsgNone = 'bnon', kMsgSelect = 'bsel' };

struct BindItem : public BListItem {
	int action;	// index in kBindActions, -1 for a category heading
	const char* heading;
	BindItem(int a, const char* h) : action(a), heading(h) {}
	void DrawItem(BView* owner, BRect frame, bool complete) override;
};

class BindList : public BListView {
public:
	BindList(Binds* binds, bool team, std::function<void()> changed)
		: BListView("bindings"), fBinds(binds), fTeam(team), fChanged(changed) {}

	std::string KeyText(int action) const
	{
		if (action == fCapturing)
			return "Press a key…";
		const char* a = kBindActions[action].action;
		auto it = fBinds->find(a);
		std::string key = it != fBinds->end() ? it->second : "";
		if (!key.empty())
			return key_label(key);
		auto g = gBinds.find(a);
		std::string own = g != gBinds.end() && !g->second.empty() ? g->second : kBindActions[action].key;
		return fTeam ? "Player's key (" + key_label(own) + ")" : key_label(own);
	}

	int SelectedAction() const
	{
		BindItem* item = (BindItem*)ItemAt(CurrentSelection());
		return item ? item->action : -1;
	}

	void Set(int action, const std::string& key)
	{
		if (action < 0)
			return;
		(*fBinds)[kBindActions[action].action] = key;
		fCapturing = -1;
		Invalidate();
		fChanged();
	}

	void Capture()
	{
		fCapturing = SelectedAction();
		MakeFocus();
		Invalidate();
	}

	void SelectionChanged() override
	{
		// headings aren't actions
		int32 i = CurrentSelection();
		BindItem* item = (BindItem*)ItemAt(i);
		if (item && item->action < 0) {
			Select(i < fLast && i > 0 ? i - 1 : i + 1);	// on past it, the way the user went
			return;
		}
		fLast = i;
		if (fCapturing >= 0 && SelectedAction() != fCapturing) {
			fCapturing = -1;
			Invalidate();
		}
		BListView::SelectionChanged();
	}

	bool InitiateDrag(BPoint, int32, bool) override { return false; }

	void KeyDown(const char* bytes, int32 count) override
	{
		if (fCapturing < 0) {
			BListView::KeyDown(bytes, count);
			return;
		}
		int32 raw = 0;
		Window()->CurrentMessage()->FindInt32("key", &raw);
		int sc = raw_key_to_scancode(raw);
		if (sc > 0 && sc < (int)gKeyNames.size() && !gKeyNames[sc].empty())
			Set(fCapturing, gKeyNames[sc]);
	}

	void MouseDown(BPoint where) override
	{
		if (fCapturing < 0) {
			BListView::MouseDown(where);
			return;
		}
		int32 buttons = 0;
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);
		Set(fCapturing, buttons & B_SECONDARY_MOUSE_BUTTON ? "mouser"
			: buttons & B_TERTIARY_MOUSE_BUTTON ? "mousem" : "mousel");
	}

	void MakeFocus(bool focus = true) override
	{
		if (!focus && fCapturing >= 0) {
			fCapturing = -1;
			Invalidate();
		}
		BListView::MakeFocus(focus);
	}

	void MessageReceived(BMessage* msg) override
	{
		if (fCapturing >= 0 && msg->what == B_MOUSE_WHEEL_CHANGED) {
			float dy = 0;
			msg->FindFloat("be:wheel_delta_y", &dy);
			if (dy != 0)
				Set(fCapturing, dy < 0 ? "wheelup" : "wheeldown");
			return;
		}
		if (fCapturing >= 0 && msg->what == B_MODIFIERS_CHANGED) {
			// modifier keys come only this way; the same scancodes as libhwhaiku
			int32 now = 0, old = 0;
			msg->FindInt32("modifiers", &now);
			msg->FindInt32("be:old_modifiers", &old);
			static const struct { uint32 bit; const char* key; } kMods[] = {
				{B_LEFT_SHIFT_KEY, "left_shift"}, {B_RIGHT_SHIFT_KEY, "right_shift"},
				{B_LEFT_CONTROL_KEY, "left_ctrl"}, {B_RIGHT_CONTROL_KEY, "right_ctrl"},
				{B_LEFT_COMMAND_KEY, "left_alt"}, {B_RIGHT_COMMAND_KEY, "right_alt"},
				{B_LEFT_OPTION_KEY, "left_gui"}, {B_RIGHT_OPTION_KEY, "right_gui"}};
			for (auto m : kMods)
				if (now & ~old & m.bit) {
					Set(fCapturing, m.key);
					return;
				}
			return;
		}
		BListView::MessageReceived(msg);
	}

private:
	Binds* fBinds;
	bool fTeam;
	std::function<void()> fChanged;
	int fCapturing = -1;
	int32 fLast = 0;
};

void BindItem::DrawItem(BView* owner, BRect frame, bool)
{
	BindList* list = (BindList*)owner;
	bool selected = IsSelected() && action >= 0;
	owner->SetLowUIColor(selected ? B_LIST_SELECTED_BACKGROUND_COLOR : B_LIST_BACKGROUND_COLOR);
	owner->FillRect(frame, B_SOLID_LOW);
	owner->SetHighUIColor(selected ? B_LIST_SELECTED_ITEM_TEXT_COLOR : B_LIST_ITEM_TEXT_COLOR);
	font_height fh;
	owner->GetFontHeight(&fh);
	float base = frame.top + fh.ascent + (frame.Height() + 1 - fh.ascent - fh.descent) / 2;
	if (action < 0) {
		owner->SetFont(be_bold_font);
		owner->DrawString(heading, BPoint(frame.left + 4, base));
		owner->SetFont(be_plain_font);
		return;
	}
	owner->DrawString(kBindActions[action].label, BPoint(frame.left + 16, base));
	// the keys line up after the longest action name
	static float keyX = -1;
	if (keyX < 0)
		for (int i = 0; i < kBindActionCount; i++)
			keyX = std::max(keyX, owner->StringWidth(kBindActions[i].label));
	owner->DrawString(list->KeyText(action).c_str(), BPoint(frame.left + 16 + keyX + 24, base));
}

class BindEditor : public BGroupView {
public:
	BindEditor(Binds* binds, bool team, std::function<void()> changed)
		: BGroupView("Controls", B_VERTICAL)
	{
		fList = new BindList(binds, team, changed);
		for (int i = 0; i < kBindActionCount; i++) {
			if (kBindActions[i].category)
				fList->AddItem(new BindItem(-1, kBindActions[i].category));
			fList->AddItem(new BindItem(i, NULL));
		}
		fList->SetSelectionMessage(new BMessage(kMsgSelect));
		fList->SetInvocationMessage(new BMessage(kMsgSet));
		fList->SetExplicitMinSize(BSize(be_plain_font->StringWidth("Pick a weapon or target")
			+ be_plain_font->StringWidth("Player's key (Right mouse button)") + 64,
			be_plain_font->Size() * 18));
		BScrollView* scroll = new BScrollView("bindings scroll", fList, 0, false, true);
		BLayoutBuilder::Group<>(this)
			.Add(scroll)
			.AddGroup(B_HORIZONTAL)
				.Add(fSet = new BButton("Set key", new BMessage(kMsgSet)))
				.Add(fNone = new BButton("No key", new BMessage(kMsgNone)))
				.Add(fDefault = new BButton(team ? "Player's key" : "Default key", new BMessage(kMsgDefault)))
				.AddGlue()
			.End();
		fList->Select(1);
	}

	void AttachedToWindow() override
	{
		BGroupView::AttachedToWindow();
		fList->SetTarget(this);
		fSet->SetTarget(this);
		fNone->SetTarget(this);
		fDefault->SetTarget(this);
	}

	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kMsgSet: fList->Capture(); break;
			case kMsgNone: fList->Set(fList->SelectedAction(), "none"); break;
			case kMsgDefault: fList->Set(fList->SelectedAction(), ""); break;
			case kMsgSelect: break;
			default: BGroupView::MessageReceived(msg);
		}
	}

private:
	BindList* fList;
	BButton *fSet, *fNone, *fDefault;
};

BView* new_bind_editor(Binds* binds, bool team, std::function<void()> changed)
{
	return new BindEditor(binds, team, changed);
}
