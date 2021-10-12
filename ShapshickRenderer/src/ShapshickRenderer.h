#pragma once
#include "DXApp.h"

class ShapShickRenderer : public DXApp
{
public:
	ShapShickRenderer(HINSTANCE hInstance);
	~ShapShickRenderer();

	virtual bool Initialize() override;

private:
	virtual void OnResize()override;
	virtual void Update(const Timer& gt) override;
	virtual void Draw(const Timer& gt) override;

};