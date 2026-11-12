//////////////////////////////////////////////////
// Interface [StatusView.h]
//////////////////////////////////////////////////

#ifndef STATUS_VIEW_H
#define STATUS_VIEW_H

#include <string>

#include <Invoker.h>
#include <StringView.h>

/** A BStringView class that will set its size to fit inside a status bar at the bottom of a
 * window (having the same height as a scrollbar).
 */
class StatusView : public BStringView, public BInvoker {
public:
						StatusView(const char *name = NULL, BBitmap* icon = NULL);
						~StatusView();

	void				Draw(BRect updateRect) override;
	void				MouseDown(BPoint) override;

private:
	BBitmap*			fIcon;
};

#endif
