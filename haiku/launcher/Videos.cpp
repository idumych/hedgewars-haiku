// frontend-qt6/ui/page/pagevideos.cpp and net/recorder.cpp, natively. The
// engine writes the camera positions and sound of a recorded stretch to
// VideoTemp/; a second engine replays the game off screen along them and
// encodes the video with the Media Kit (libhwhaiku's avwrapper).
#include "Videos.h"
#include "Game.h"

#include <Alert.h>
#include <Bitmap.h>
#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Node.h>
#include <Roster.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <algorithm>
#include <string.h>

enum { kMsgVideoSel = 'vsel', kMsgVideoPlay = 'vply', kMsgVideoTrash = 'vtrs',
	kMsgVideoProgress = 'vprg', kMsgVideoDone = 'vdon' };

static BPath user_path(const std::string& rel) { return BPath(gUserDir.Path(), rel.c_str()); }

// the pairs Haiku's ffmpeg encodes (no H.264 encoder; Opus refuses avwrapper's quality setting)
static const struct { const char* label; const char* format; const char* video; const char* audio; } kFormats[] = {
	{"MP4 (MPEG-4, AAC)", "mp4", "mpeg4", "aac"},
	{"WebM (VP8, Vorbis)", "webm", "libvpx", "libvorbis"},
	{"Ogg (Theora, Vorbis)", "ogg", "libtheora", "libvorbis"},
};
static const int kFps[] = {24, 30, 60};

static BMenuField* choice(const char* label, const std::vector<std::string>& items, int marked)
{
	BPopUpMenu* menu = new BPopUpMenu("choice");
	for (size_t i = 0; i < items.size(); i++)
		menu->AddItem(new BMenuItem(items[i].c_str(), new BMessage('vset')));
	if (BMenuItem* item = menu->ItemAt(marked))
		item->SetMarked(true);
	return new BMenuField(label, menu);
}

static int marked(BMenuField* f) { return f->Menu()->IndexOf(f->Menu()->FindMarked()); }

static BPath settings_path() { return user_path("video_settings"); }

// the thumbnail the engine saved at the start of the recording
class VideoPicture : public BView {
public:
	VideoPicture() : BView("video picture", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
	{
		SetExplicitSize(BSize(200, 112));
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}
	~VideoPicture() { delete fBitmap; }
	void SetPicture(const char* path)
	{
		delete fBitmap;
		fBitmap = path ? BTranslationUtils::GetBitmap(path) : NULL;
		Invalidate();
	}
	void Draw(BRect) override
	{
		if (!fBitmap)
			return;
		BRect b = fBitmap->Bounds(), r = Bounds();
		float scale = std::min(r.Width() / b.Width(), r.Height() / b.Height());
		BRect dst(0, 0, b.Width() * scale, b.Height() * scale);
		dst.OffsetBy((r.Width() - dst.Width()) / 2, (r.Height() - dst.Height()) / 2);
		DrawBitmap(fBitmap, b, dst, B_FILTER_BITMAP_BILINEAR);
	}
private:
	BBitmap* fBitmap = NULL;
};

VideosView::VideosView() : BGroupView("Videos", B_VERTICAL)
{
	for (const char* d : {"Videos", "VideoTemp", "VideoThumbnails"})
		create_directory(user_path(d).Path(), 0755);
	// leftovers of renderings cut short when the launcher last quit (PageVideos does the same)
	BDirectory temp(user_path("VideoTemp").Path());
	BEntry e;
	while (temp.GetNextEntry(&e) == B_OK)
		e.Remove();

	fList = new BListView("videos");
	fList->SetSelectionMessage(new BMessage(kMsgVideoSel));
	fList->SetInvocationMessage(new BMessage(kMsgVideoPlay));
	fList->SetExplicitMinSize(BSize(300, 170));
	BScrollView* ls = new BScrollView("videos scroll", fList, 0, false, true);
	fText = new BTextView("video description");
	fText->MakeEditable(false);
	fText->SetWordWrap(true);
	fText->SetExplicitMinSize(BSize(200, 60));
	fPicture = new VideoPicture;
	BStringView* hint = new BStringView("hint", "Press R during a game to start and stop recording a video.");
	hint->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fPlay = new BButton("Play", new BMessage(kMsgVideoPlay));
	fTrash = new BButton("Move to Trash", new BMessage(kMsgVideoTrash));
	BMessage settings;
	BFile file(settings_path().Path(), B_READ_ONLY);
	settings.Unflatten(&file);
	std::vector<std::string> formats, rates;
	for (const auto& f : kFormats)
		formats.push_back(f.label);
	for (int r : kFps)
		rates.push_back(std::to_string(r) + " frames per second");
	fFormat = choice("Format:", formats, std::clamp(settings.GetInt32("format", 0), 0, 2));
	fFps = choice("Frame rate:", rates, std::clamp(settings.GetInt32("fps", 1), 0, 2));
	BLayoutBuilder::Group<>(this)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(hint)
		.AddGroup(B_HORIZONTAL)
			.Add(ls, 2)
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 1)
				.Add(fPicture)
				.Add(fText)
				.AddGlue()
			.End()
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fFormat)
			.Add(fFps)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fTrash)
			.AddGlue()
			.Add(fPlay)
		.End();
}

void VideosView::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	fList->SetTarget(this);
	fPlay->SetTarget(this);
	fTrash->SetTarget(this);
	fFormat->Menu()->SetTargetForItems(this);
	fFps->Menu()->SetTargetForItems(this);
	Fill();
}

void VideosView::Fill()
{
	int sel = fList->CurrentSelection();
	std::string selected = sel >= 0 && sel < (int)fFiles.size() ? fFiles[sel] : "";
	while (BListItem* item = fList->RemoveItem((int32)0))
		delete item;
	fFiles.clear();
	for (const std::string& label : fEncoding)
		fList->AddItem(new BStringItem(label.c_str()));
	std::vector<std::string> names = list_dir("Videos", NULL, false, true);
	for (auto n = names.rbegin(); n != names.rend(); ++n) {	// newest first
		fFiles.push_back(user_path("Videos/" + *n).Path());
		fList->AddItem(new BStringItem(n->c_str()));
	}
	auto at = std::find(fFiles.begin(), fFiles.end(), selected);
	fList->Select(at != fFiles.end() ? fEncoding.size() + (at - fFiles.begin()) : 0);
	ShowSelected();
}

// the description avwrapper keeps on the file: "Key: value" lines, then prefix[...]prefix
void VideosView::ShowSelected()
{
	int i = fList->CurrentSelection() - (int)fEncoding.size();
	bool video = i >= 0 && i < (int)fFiles.size();
	fPlay->SetEnabled(video);
	fTrash->SetEnabled(video);
	std::string desc, prefix;
	if (video) {
		char buf[1024] = "";
		ssize_t n = BNode(fFiles[i].c_str()).ReadAttr("Hedgewars:description", B_STRING_TYPE, 0, buf, sizeof(buf) - 1);
		desc.assign(buf, n > 0 ? n : 0);
		desc = desc.c_str();
		size_t p = desc.find("prefix[");
		if (p != std::string::npos) {
			prefix = desc.substr(p + 7, desc.find("]prefix", p) - p - 7);
			desc.erase(p);
		}
	}
	fText->SetText(desc.c_str());
	// .png from the Pascal engine, .bmp from the C one (pas2c has no PNG writer)
	std::string thumb = user_path("VideoThumbnails/" + prefix + ".png").Path();
	if (!BEntry(thumb.c_str()).Exists())
		thumb = user_path("VideoThumbnails/" + prefix + ".bmp").Path();
	fPicture->SetPicture(prefix.empty() ? NULL : thumb.c_str());
}

struct Rendering {
	std::string prefix, record;
	BMessenger to;
	int format, fps;
};

static int32 render_thread(void* data)
{
	Rendering* r = (Rendering*)data;
	Game g;
	g.replay = r->record;
	for (const char* kind : {"\x02TD", "\x02TL", "\x02TN", "\x02TS"}) {	// HWRecorder::EncodeVideo
		size_t at = g.replay.find(kind);
		if (at != std::string::npos)
			g.replay.replace(at, 3, "\x02TV");
	}
	g.videoPrefix = r->prefix;
	g.videoFormat = kFormats[r->format].format;
	g.videoCodec = kFormats[r->format].video;
	g.audioCodec = kFormats[r->format].audio;
	g.videoFps = kFps[r->fps];
	g.videoSound = BEntry(user_path("VideoTemp/" + r->prefix + ".sw").Path()).Exists();
	BMessenger to = r->to;
	std::string prefix = r->prefix;
	g.progress = [to, prefix](float f) {
		BMessage m(kMsgVideoProgress);
		m.AddString("prefix", prefix.c_str());
		m.AddFloat("progress", f);
		to.SendMessage(&m);
	};
	Outcome o = run_game(g);
	BMessage done(kMsgVideoDone);
	done.AddString("prefix", prefix.c_str());
	// the finished video: the one file in VideoTemp named after the prefix that isn't our input
	BDirectory temp(user_path("VideoTemp").Path());
	BEntry e;
	char name[B_FILE_NAME_LENGTH];
	bool moved = false;
	while (temp.GetNextEntry(&e) == B_OK) {
		e.GetName(name);
		std::string n = name;
		if (n.compare(0, prefix.size() + 1, prefix + ".") != 0)
			continue;
		std::string ext = n.substr(prefix.size() + 1);
		if (o.encoded && ext != "txtin" && ext != "sw" && ext != "hwd") {
			BDirectory videos(user_path("Videos").Path());
			moved = e.MoveTo(&videos) == B_OK;
		} else
			e.Remove();
	}
	if (!moved)
		done.AddString("error", o.engineError.empty() ? o.error.c_str() : o.engineError.c_str());
	to.SendMessage(&done);
	delete r;
	return 0;
}

int VideosView::StartEncoding(const std::string& record)
{
	BDirectory temp(user_path("VideoTemp").Path());
	BEntry e;
	char name[B_FILE_NAME_LENGTH];
	std::vector<std::string> prefixes;
	while (temp.GetNextEntry(&e) == B_OK) {
		e.GetName(name);
		std::string n = name;
		if (n.size() > 7 && n.compare(n.size() - 7, 7, ".txtout") == 0) {
			prefixes.push_back(n.substr(0, n.size() - 7));
			e.Rename((prefixes.back() + ".txtin").c_str());	// so it isn't picked up twice
		}
	}
	for (const std::string& p : prefixes) {
		fEncoding.push_back(p + " (rendering…)");
		resume_thread(spawn_thread(render_thread, "video", B_LOW_PRIORITY,
			new Rendering{p, record, BMessenger(this), marked(fFormat), marked(fFps)}));
	}
	if (!prefixes.empty())
		Fill();
	return prefixes.size();
}

void VideosView::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case kMsgVideoSel: ShowSelected(); break;
		case 'vset': {	// remembered for the next recording
			BMessage settings;
			settings.AddInt32("format", marked(fFormat));
			settings.AddInt32("fps", marked(fFps));
			save_message(settings, settings_path());
			break;
		}
		case kMsgVideoPlay: {
			int i = fList->CurrentSelection() - (int)fEncoding.size();
			entry_ref ref;
			if (i >= 0 && i < (int)fFiles.size() && get_ref_for_path(fFiles[i].c_str(), &ref) == B_OK)
				be_roster->Launch(&ref);
			break;
		}
		case kMsgVideoTrash: {
			int i = fList->CurrentSelection() - (int)fEncoding.size();
			if (i >= 0 && i < (int)fFiles.size())
				move_to_trash(fFiles[i].c_str());
			Fill();
			break;
		}
		case kMsgVideoProgress:
		case kMsgVideoDone: {
			const char* prefix = msg->GetString("prefix", "");
			for (size_t i = 0; i < fEncoding.size(); i++) {
				if (fEncoding[i].compare(0, strlen(prefix) + 1, std::string(prefix) + " ") != 0)
					continue;
				if (msg->what == kMsgVideoDone)
					fEncoding.erase(fEncoding.begin() + i);
				else {
					fEncoding[i] = std::string(prefix) + " (rendering, "
						+ std::to_string((int)(msg->GetFloat("progress", 0) * 100)) + "%)";
					if (BStringItem* item = (BStringItem*)fList->ItemAt(i)) {
						item->SetText(fEncoding[i].c_str());
						fList->InvalidateItem(i);
					}
				}
				break;
			}
			if (msg->what == kMsgVideoDone) {
				Fill();
				if (fEncoding.empty())	// the window's status line said a video was being made
					Window()->PostMessage(kMsgVideosReady);
				const char* error;
				if (msg->FindString("error", &error) == B_OK)
					(new BAlert("Hedgewars", (std::string("The video could not be made.")
						+ (error[0] ? std::string("\n\n") + error : "")).c_str(), "OK", NULL, NULL,
						B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go(NULL);
			}
			break;
		}
		default:
			BGroupView::MessageReceived(msg);
	}
}
