// The Videos tab (frontend-qt6's PageVideos): videos recorded in games with
// the record key (R), rendered off screen by a second engine after the game.
#pragma once

#include <GroupView.h>

#include <string>
#include <vector>

class BButton;
class BListView;
class BMenuField;
class BTextView;
class VideoPicture;

class VideosView : public BGroupView {
public:
	VideosView();
	void AttachedToWindow() override;
	void MessageReceived(BMessage* msg) override;
	void Fill();
	BButton* PlayButton() const { return fPlay; }
	// after a game: renders each recording the engine started (VideoTemp/*.txtout); how many
	int StartEncoding(const std::string& record);

private:
	void ShowSelected();	// the selected video's picture and description
	BListView* fList;
	BTextView* fText;
	VideoPicture* fPicture;
	BButton *fPlay, *fTrash;
	BMenuField *fFormat, *fFps;
	std::vector<std::string> fFiles;	// paths of the videos listed
	std::vector<std::string> fEncoding;	// labels of the videos being rendered
};
