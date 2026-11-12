/*
 * Copyright 19??-2001, John Blanco
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef PICTURE_VIEW_H
#define PICTURE_VIEW_H

#include <Bitmap.h>
#include <View.h>

class PictureView : public BView {
public:
			  PictureView(const char *filename, uint32 flags = B_WILL_DRAW);
			  PictureView(BPositionIO *source, uint32 flags = B_WILL_DRAW);
   	         ~PictureView();

	void	SetBitmap(BPositionIO *source);

	void      AttachedToWindow() override;
	void      Draw(BRect frame) override;

	bool      InitCheck();
	
private:
	void      _Init();
private:
	BBitmap *_bitmap;
};

#endif
