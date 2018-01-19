#include "pch.h"
#include "peer_view.h"
using namespace StreamingToolkit;

void* PeerView::operator new(size_t i)
{
	return _mm_malloc(i, 16);
}

void PeerView::operator delete(void* p)
{
	_mm_free(p);
}

bool PeerView::IsValid()
{
	return lookAt != 0 && up != 0 && eye != 0 &&
		!DirectX::XMVector3Equal(eye, DirectX::XMVectorZero());
}