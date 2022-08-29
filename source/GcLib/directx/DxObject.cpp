#include "source/GcLib/pch.h"

#include "DxScript.hpp"
#include "DxUtility.hpp"
#include "DirectGraphics.hpp"
#include "DirectInput.hpp"
#include "MetasequoiaMesh.hpp"
//#include "ElfreinaMesh.hpp"

using namespace gstd;
using namespace directx;

//****************************************************************************
//DxScriptObjectBase
//****************************************************************************
DxScriptObjectBase::DxScriptObjectBase() {
	manager_ = nullptr;

	idObject_ = DxScript::ID_INVALID;
	idScript_ = ScriptClientBase::ID_SCRIPT_FREE;
	typeObject_ = TypeObject::Base;

	bDelete_ = false;
	bActive_ = false;
	bVisible_ = true;
	priRender_ = 50;
	
	frameExist_ = 0;
}
DxScriptObjectBase::~DxScriptObjectBase() {
	//if (manager_ != nullptr && idObject_ != DxScript::ID_INVALID)
	//	manager_->listUnusedIndex_.push_back(idObject_);
}

void DxScriptObjectBase::SetRenderPriority(double pri) {
	priRender_ = pri * (manager_->GetRenderBucketCapacity() - 1U);
}
double DxScriptObjectBase::GetRenderPriority() {
	return (double)priRender_ / (manager_->GetRenderBucketCapacity() - 1U);
}

//****************************************************************************
//DxSplineObject
//****************************************************************************
DxSplineObject::DxSplineObject() {
	typeObject_ = TypeObject::Spline;

	bUpdate_ = true;
}

void DxSplineObject::clear() {
	nodes_.clear();
	arcTable_.clear();

	bUpdate_ = true;
}

void DxSplineObject::AddNode(DxSplineObjectNode node) {
	nodes_.push_back(node);

	bUpdate_ = true;
}

void DxSplineObject::SetNode(DxSplineObjectNode node, size_t index) {
	if (index < nodes_.size())
		nodes_[index] = node;
	else
		nodes_.push_back(node);

	bUpdate_ = true;
}

void DxSplineObject::CatmullRomOrder1(DxSplineObjectNode nodeA, DxSplineObjectNode nodeB, size_t index) {
	nodes_[index][3] = nodeB[0] - nodeA[0];
	nodes_[index][4] = nodeB[1] - nodeA[1];
	nodes_[index][5] = nodeB[2] - nodeA[2];
}

void DxSplineObject::CatmullRomOrder2(DxSplineObjectNode nodeA, DxSplineObjectNode nodeB, DxSplineObjectNode nodeC, size_t index, bool flip) {
	nodes_[index][3] = ((nodeB[0] - nodeA[0]) * 2.0 - (nodeC[0] - nodeA[0]) / 2.0) * (1.0 - 2.0 * flip);
	nodes_[index][4] = ((nodeB[1] - nodeA[1]) * 2.0 - (nodeC[1] - nodeA[1]) / 2.0) * (1.0 - 2.0 * flip);
	nodes_[index][5] = ((nodeB[2] - nodeA[2]) * 2.0 - (nodeC[2] - nodeA[2]) / 2.0) * (1.0 - 2.0 * flip);
}

void DxSplineObject::CatmullRom() {
	if (nodes_.size() < 2)
		return;

	if (nodes_.size() == 2) {
		DxSplineObject::CatmullRomOrder1(nodes_[0], nodes_[1], 0);
		DxSplineObject::CatmullRomOrder1(nodes_[0], nodes_[1], 1);
	}

	else {
		size_t s = nodes_.size();
		
		DxSplineObject::CatmullRomOrder2(nodes_[0], nodes_[1], nodes_[2], 0, false);
		DxSplineObject::CatmullRomOrder2(nodes_[s - 1], nodes_[s - 2], nodes_[s - 3], s - 1, true);

		for (size_t i = 1; i < nodes_.size() - 1; ++i) {
			DxSplineObjectNode n0 = nodes_[i - 1];
			DxSplineObjectNode n2 = nodes_[i + 1];

			nodes_[i][3] = (n2[0] - n0[0]) / 2.0;
			nodes_[i][4] = (n2[1] - n0[1]) / 2.0;
			nodes_[i][5] = (n2[2] - n0[2]) / 2.0;
		}
	}
	
	bUpdate_ = true;
}

void DxSplineObject::ComputeArcTable() {
	arcTable_.clear();

	double arc = 0;

	arcTable_.push_back(arc);

	size_t curveCount = nodes_.size() - 1;

	if (curveCount == 0)
		return;

	size_t iter = curveCount * ARC_PRECISION;
	double incr = 1.0 / (double)iter;

	for (size_t i = 0; i < iter; ++i) {
		DxSplineObjectNode n0 = DxSplineObject::Lerp(i * incr);
		DxSplineObjectNode n1 = DxSplineObject::Lerp(((double)i + 1) * incr);

		double dx = n1[0] - n0[0];
		double dy = n1[1] - n0[1];
		double dz = n1[2] - n0[2];

		arc += sqrt(dx * dx + dy * dy + dz * dz);

		arcTable_.push_back(arc);
	}

	bUpdate_ = false;
}

double DxSplineObject::GetArcLength(double t) {
	if (bUpdate_)
		DxSplineObject::ComputeArcTable();

	if (arcTable_.size() <= 1)
		return 0;

	t = std::clamp(t, 0.0, 1.0);
	t *= arcTable_.size() - 1;

	if (t - (size_t)t == 0)
		return arcTable_[(size_t)t];

	double t0 = floor(t);
	double t1 = ceil(t);

	double d0 = arcTable_[(size_t)t0];
	double d1 = arcTable_[(size_t)t1];

	return d0 + (d1 - d0) * (t - t0) / (t1 - t0);
}

DxSplineObjectNode DxSplineObject::Lerp(double t) {
	DxSplineObjectNode res(NODE);

	t = std::clamp(t, 0.0, 1.0);

	double tSpan = t * (nodes_.size() - 1);

	DxSplineObjectNode n0 = nodes_[(size_t)floor(tSpan)];
	DxSplineObjectNode n1 = nodes_[(size_t)ceil(tSpan)];

	t = fmod(tSpan, 1.0);

	// Position interpolation

	res[0] = DxSplineObject::f1(t) * n0[0] + DxSplineObject::f2(t) * n1[0] + DxSplineObject::f3(t) * n0[3] + DxSplineObject::f4(t) * n1[3];
	res[1] = DxSplineObject::f1(t) * n0[1] + DxSplineObject::f2(t) * n1[1] + DxSplineObject::f3(t) * n0[4] + DxSplineObject::f4(t) * n1[4];
	res[2] = DxSplineObject::f1(t) * n0[2] + DxSplineObject::f2(t) * n1[2] + DxSplineObject::f3(t) * n0[5] + DxSplineObject::f4(t) * n1[5];

	// Tangent interpolation

	res[3] = DxSplineObject::g1(t) * n0[0] + DxSplineObject::g2(t) * n1[0] + DxSplineObject::g3(t) * n0[3] + DxSplineObject::g4(t) * n1[3];
	res[4] = DxSplineObject::g1(t) * n0[1] + DxSplineObject::g2(t) * n1[1] + DxSplineObject::g3(t) * n0[4] + DxSplineObject::g4(t) * n1[4];
	res[5] = DxSplineObject::g1(t) * n0[2] + DxSplineObject::g2(t) * n1[2] + DxSplineObject::g3(t) * n0[5] + DxSplineObject::g4(t) * n1[5];

	return res;
}

DxSplineObjectNode DxSplineObject::LerpArc(double t) {
	if (bUpdate_)
		DxSplineObject::ComputeArcTable();

	if (arcTable_.size() == 1)
		return DxSplineObject::Lerp(0);

	t = std::clamp(t, 0.0, 1.0);

	double dMax = arcTable_.back();
	double dist = t * dMax;

	size_t n = arcTable_.size();

	for (size_t iLow = 0; iLow < n - 1; ++iLow) {
		size_t iHigh = iLow + 1;

		double dLow = arcTable_[iLow];
		double dHigh = arcTable_[iHigh];

		if (dLow <= dist && dist <= dHigh) {
			double iBest = iLow + (iHigh - iLow) * ((dist - dLow) / (dHigh - dLow));
			return DxSplineObject::Lerp(iBest / double(n - 1));
		}
	}

	return DxSplineObject::Lerp(dist / dMax);
}

//****************************************************************************
//DxSpringMassSystemObject
//****************************************************************************
DxSpringMassSystemObject::DxSpringMassSystemObject() {
	typeObject_ = TypeObject::SpringMassSystem;
}

void DxSpringMassSystemObject::SetParticle(DxSpringMassSystemObjectParticle particle, size_t index) {
	if (index < particles_.size())
		particles_[index] = particle;
	else
		particles_.push_back(particle);
}

void DxSpringMassSystemObject::RemoveParticle(size_t index) {
	particles_.erase(particles_.begin() + index);

	// Any springs that shared this particle must now be deleted
	for (size_t i = 0; i < springs_.size(); ) {
		DxSpringMassSystemObjectSpring* s = &(springs_[i]);
		if (s->a == index || s->b == index)
			springs_.erase(springs_.begin() + i);
		else
			++i;
	}

	// Any springs who contained index references greater than the removal index must now shift down by one to accomodate the loss
	for (size_t i = 0; i < springs_.size(); ++i) {
		DxSpringMassSystemObjectSpring* s = &(springs_[i]);
		if (s->a > index)
			--s->a;
		if (s->b > index)
			--s->b;
	}
}

void DxSpringMassSystemObject::AddSpring(DxSpringMassSystemObjectSpring spring) {
	if (spring.rest < 0) {
		DxSpringMassSystemObjectParticle* pA = &(particles_[spring.a]);
		DxSpringMassSystemObjectParticle* pB = &(particles_[spring.b]);

		DxVector3::DxVec3 vecAB;
		DxVector3::DxVec3Sub(vecAB, pB->pos, pA->pos);

		double norm = DxVector3::DxVec3Norm(vecAB);
		spring.rest = norm;
	}

	springs_.push_back(spring);
}

void DxSpringMassSystemObject::SetSpring(DxSpringMassSystemObjectSpring spring, size_t index) {
	if (spring.rest < 0) {
		DxSpringMassSystemObjectParticle* pA = &(particles_[spring.a]);
		DxSpringMassSystemObjectParticle* pB = &(particles_[spring.b]);

		DxVector3::DxVec3 vecAB;
		DxVector3::DxVec3Sub(vecAB, pB->pos, pA->pos);

		double norm = DxVector3::DxVec3Norm(vecAB);
		spring.rest = norm;
	}

	if (index < springs_.size())
		springs_[index] = spring;
	else
		springs_.push_back(spring);
}

void DxSpringMassSystemObject::SetPlane(DxSpringMassSystemObjectPlane plane, size_t index) {
	if (index < planes_.size())
		planes_[index] = plane;
	else
		planes_.push_back(plane);
}

void DxSpringMassSystemObject::SpringForce(
	DxVector3::DxVec3 out, DxSpringMassSystemObjectParticle* pA, DxSpringMassSystemObjectParticle* pB,
	double ks, double kd, double rest
) {
	DxVector3::DxVec3 dba;
	DxVector3::DxVec3Sub(dba, pA->pos, pB->pos);

	double mba = DxVector3::DxVec3Norm(dba);

	DxVector3::DxVec3 fs;
	DxVector3::DxVec3Scale(fs, dba, ks * (rest - mba) / std::max(mba, 0.001));

	DxVector3::DxVec3 vba;
	DxVector3::DxVec3Sub(vba, pA->vel, pB->vel);
	DxVector3::DxVec3Scale(dba, dba, 1.0 / std::max(mba, 0.001));
	DxVector3::DxVec3Dot(vba, vba, dba);

	double inner = DxVector3::DxVec3Sum(vba);

	DxVector3::DxVec3 fd;
	DxVector3::DxVec3Scale(fd, dba, -(kd) * inner);

	DxVector3::DxVec3Add(out, fs, fd);
}

void DxSpringMassSystemObject::Integrate() {
	// Compute per-particle force components
	for (size_t i = 0; i < particles_.size(); ++i) {
		DxSpringMassSystemObjectParticle* p = &(particles_[i]);

		DxVector3::DxVec3Set(p->force, 0, 0, 0);

		if (p->bMove) {
			// Add drag
			DxVector3::DxVec3 drag;
			DxVector3::DxVec3Scale(drag, p->vel, -globalDrag_);
			DxVector3::DxVec3Add(p->force, p->force, drag);

			// Add gravity
			DxVector3::DxVec3 gravity;
			DxVector3::DxVec3Scale(gravity, gravity_, p->mass);
			DxVector3::DxVec3Add(p->force, p->force, gravity);

			// Add penalty forces against planes
			for (size_t j = 0; j < planes_.size(); ++j) {
				DxSpringMassSystemObjectPlane* q = &(planes_[j]);

				DxVector3::DxVec3 v;
				DxVector3::DxVec3Sub(v, p->pos, q->pos);

				DxVector3::DxVec3 dot;
DxVector3::DxVec3Dot(dot, v, q->normal);

double signedDist = DxVector3::DxVec3Sum(dot);

if (signedDist < q->epsilon) {
	DxVector3::DxVec3 projPos;
	DxVector3::DxVec3Copy(projPos, p->pos);

	DxVector3::DxVec3 projTravel;
	DxVector3::DxVec3Scale(projTravel, q->normal, -(signedDist - q->epsilon));
	DxVector3::DxVec3Add(projPos, projPos, projTravel);

	DxSpringMassSystemObjectParticle pPlane = DxSpringMassSystemObjectParticle(projPos, DxVector3::DxVec3{ 0.0, 0.0, 0.0 }, 1.0, false);

	// Add penalty spring force against the plane
	DxVector3::DxVec3 springForce;
	SpringForce(springForce, p, &pPlane, q->ks, q->kd, 0.0);
	DxVector3::DxVec3Scale(springForce, springForce, FRAME_STEP);
	DxVector3::DxVec3Add(p->force, p->force, springForce);
}
			}
		}
	}

	// Compute per-spring force components
	for (size_t i = 0; i < springs_.size(); ++i) {
		DxSpringMassSystemObjectSpring* s = &(springs_[i]);

		// Add spring force
		DxVector3::DxVec3 springForce;
		SpringForce(springForce, &(particles_[s->a]), &(particles_[s->b]), s->ks, s->kd, s->rest);
		DxVector3::DxVec3Scale(springForce, springForce, FRAME_STEP);

		DxSpringMassSystemObjectParticle* pA = &(particles_[s->a]);
		DxSpringMassSystemObjectParticle* pB = &(particles_[s->b]);

		if (pA->bMove) {
			DxVector3::DxVec3Add(pA->force, pA->force, springForce);
		}

		if (pB->bMove) {
			DxVector3::DxVec3Scale(springForce, springForce, -1.0);
			DxVector3::DxVec3Add(pB->force, pB->force, springForce);
		}
	}

	// Symplectic Euler Integration step

	// Update velocity
	for (size_t i = 0; i < particles_.size(); ++i) {
		DxSpringMassSystemObjectParticle* p = &(particles_[i]);

		if (p->bMove) {
			DxVector3::DxVec3 acceleration;
			DxVector3::DxVec3Scale(acceleration, p->force, 1.0 / p->mass);
			DxVector3::DxVec3Add(p->vel, p->vel, acceleration);
		}
	}

	// Update position
	for (size_t i = 0; i < particles_.size(); ++i) {
		DxSpringMassSystemObjectParticle* p = &(particles_[i]);

		if (p->bMove) {
			DxVector3::DxVec3Add(p->pos, p->pos, p->vel);
		}
	}
}

//****************************************************************************
//DxScriptSpriteAnimation
//****************************************************************************
DxScriptSpriteAnimation::DxScriptSpriteAnimation() {
	animation_ = ANIM_INVALID;
	animFrame_ = 0;
	frame_ = 0;
	loopCount_ = 0;
	loopMax_ = 0;
	bReverse_ = false;
	bActive_ = false;
}
void DxScriptSpriteAnimation::AddFrame(int id, int length, DxRect<int>& rect) {
	if (id > ANIM_INVALID) {
		if (listAnim_.count(id) == 0)
			listAnim_[id] = AnimationSequence();

		AnimationSequence& seq = listAnim_[id];
		AnimationFrame frame;
		frame.length = length;
		frame.rect = rect;
		seq.push_back(frame);
	}
}
void DxScriptSpriteAnimation::Start(int id, bool bReverse, int loop) {
	animFrame_ = 0;
	frame_ = 0;
	loopCount_ = 0;
	bReverse_ = bReverse;
	loopMax_ = loop;
	if (listAnim_.count(id) > 0) {
		animation_ = id;
		bActive_ = true;
	}
	else {
		animation_ = ANIM_INVALID;
		bActive_ = false;
	}	
}
void DxScriptSpriteAnimation::Stop() {
	animation_ = ANIM_INVALID;
	animFrame_ = 0;
	frame_ = 0;
	loopCount_ = 0;
	loopMax_ = 0;
	bReverse_ = false;
	bActive_ = false;
}
void DxScriptSpriteAnimation::Resume() {
	if (animation_ > ANIM_INVALID)
		bActive_ = true;
}
void DxScriptSpriteAnimation::Clear(int id) {
	if (id > ANIM_INVALID) {
		if (animation_ == id) Stop();
		listAnim_.erase(id);
	}
}
void DxScriptSpriteAnimation::ClearAll() {
	if (animation_ > ANIM_INVALID)
		Stop();

	listAnim_.clear();
}
void DxScriptSpriteAnimation::Animate() {
	if (bActive_) {
		RenderObject* sprite = sprite_.get();
		AnimationSequence& sequence = listAnim_[animation_];
		int frameCount = sequence.size();

		// Adjust the "real" frame index used for accessing the correct rect
		int index = bReverse_ ? (frameCount - animFrame_ - 1) : animFrame_;

		AnimationFrame& frame = sequence[index];
		int frameLength = frame.length;
		DxRect<int>& frameRect = frame.rect;

		if (Sprite2D* spr = dynamic_cast<Sprite2D*>(sprite))
			spr->SetSourceRect(frameRect);
		else if (Sprite3D* spr = dynamic_cast<Sprite3D*>(sprite))
			spr->SetSourceRect(frameRect);

		++frame_;

		// Proceed to next animation frame
		if (frame_ >= frameLength) {
			frame_ = 0;
			++animFrame_;

			// Proceed to next loop
			if (animFrame_ >= frameCount) {
				animFrame_ = 0;
				++loopCount_;

				// If a loop limit is set and reached, exit from the animation and return to default if it exists
				if (loopMax_ > 0 && loopCount_ >= loopMax_) {
					bActive_ = false;
				}
			}
		}
	}
}

//****************************************************************************
//DxScriptRenderObject
//****************************************************************************
DxScriptRenderObject::DxScriptRenderObject() {
	bZWrite_ = false;
	bZTest_ = false;
	bFogEnable_ = false;
	typeBlend_ = MODE_BLEND_ALPHA;
	modeCulling_ = D3DCULL_NONE;
	position_ = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	angle_ = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	scale_ = D3DXVECTOR3(1.0f, 1.0f, 1.0f);

	color_ = 0xffffffff;

	filterMin_ = D3DTEXF_LINEAR;
	filterMag_ = D3DTEXF_LINEAR;
	filterMip_ = D3DTEXF_NONE;
	bVertexShaderMode_ = false;
	bEnableMatrix_ = true;
}

//****************************************************************************
//DxScriptShaderObject
//****************************************************************************
DxScriptShaderObject::DxScriptShaderObject() {
	typeObject_ = TypeObject::Shader;
}

//****************************************************************************
//DxScriptPrimitiveObject
//****************************************************************************
DxScriptPrimitiveObject::DxScriptPrimitiveObject() {
	angX_ = D3DXVECTOR2(1, 0);
	angY_ = D3DXVECTOR2(1, 0);
	angZ_ = D3DXVECTOR2(1, 0);
}

void DxScriptPrimitiveObject::SetAngleX(float x) {
	x = gstd::Math::DegreeToRadian(x);
	if (angle_.x != x) {
		angle_.x = x;
		angX_ = D3DXVECTOR2(cosf(-x), sinf(-x));
	}
}
void DxScriptPrimitiveObject::SetAngleY(float y) {
	y = gstd::Math::DegreeToRadian(y);
	if (angle_.y != y) {
		angle_.y = y;
		angY_ = D3DXVECTOR2(cosf(-y), sinf(-y));
	}
}
void DxScriptPrimitiveObject::SetAngleZ(float z) {
	z = gstd::Math::DegreeToRadian(z);
	if (angle_.z != z) {
		angle_.z = z;
		angZ_ = D3DXVECTOR2(cosf(-z), sinf(-z));
	}
}

//****************************************************************************
//DxScriptPrimitiveObject2D
//****************************************************************************
DxScriptPrimitiveObject2D::DxScriptPrimitiveObject2D() {
	typeObject_ = TypeObject::Primitive2D;
	objRender_ = std::make_shared<RenderObjectTLX>();
	objRender_->SetDxObjectReference(this);

	bZWrite_ = false;
	bZTest_ = false;
}

void DxScriptPrimitiveObject2D::Render() {
	if (RenderObjectTLX* obj = GetObjectPointer()) {
		DirectGraphics* graphics = DirectGraphics::GetBase();
		DWORD bEnableFog = FALSE;
		graphics->GetDevice()->GetRenderState(D3DRS_FOGENABLE, &bEnableFog);
		if (bEnableFog)
			graphics->SetFogEnable(false);

		SetRenderState();
		obj->Render(angX_, angY_, angZ_);

		if (bEnableFog)
			graphics->SetFogEnable(true);
	}
}
void DxScriptPrimitiveObject2D::SetRenderState() {
	DirectGraphics* graphics = DirectGraphics::GetBase();
	RenderObjectTLX* obj = GetObjectPointer();
	obj->SetPosition(position_);
	obj->SetAngle(angle_);
	obj->SetScale(scale_);
	graphics->SetLightingEnable(false);
	graphics->SetZWriteEnable(false);
	graphics->SetZBufferEnable(false);
	graphics->SetBlendMode(typeBlend_);
	graphics->SetCullingMode(modeCulling_);
	graphics->SetTextureFilter(filterMin_, filterMag_, filterMip_);
	obj->SetVertexShaderRendering(bVertexShaderMode_);
	obj->SetDisableMatrixTransformation(!bEnableMatrix_);
}

void DxScriptPrimitiveObject2D::SetColor(int r, int g, int b) {
	RenderObjectTLX* obj = GetObjectPointer();
	__m128i c = Vectorize::Set(color_ >> 24, r, g, b);
	color_ = ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPacked(c));
	obj->SetColorRGB(color_);
}
void DxScriptPrimitiveObject2D::SetAlpha(int alpha) {
	RenderObjectTLX* obj = GetObjectPointer();
	ColorAccess::ClampColor(alpha);
	obj->SetAlpha(alpha);
	color_ = (color_ & 0x00ffffff) | ((byte)alpha << 24);
}

bool DxScriptPrimitiveObject2D::IsValidVertexIndex(size_t index) {
	return index >= 0 && index < GetObjectPointer()->GetVertexCount();
}
void DxScriptPrimitiveObject2D::SetVertexPosition(size_t index, float x, float y, float z) {
	RenderObjectTLX* obj = GetObjectPointer();
	obj->SetVertexPosition(index, x, y, z);
}
void DxScriptPrimitiveObject2D::SetVertexUV(size_t index, float u, float v) {
	RenderObjectTLX* obj = GetObjectPointer();
	obj->SetVertexUV(index, u, v);
}
void DxScriptPrimitiveObject2D::SetVertexAlpha(size_t index, int alpha) {
	RenderObjectTLX* obj = GetObjectPointer();
	ColorAccess::ClampColor(alpha);
	obj->SetVertexAlpha(index, alpha);
}
void DxScriptPrimitiveObject2D::SetVertexColor(size_t index, int r, int g, int b) {
	RenderObjectTLX* obj = GetObjectPointer();
	__m128i c = Vectorize::Set(0, r, g, b);
	obj->SetVertexColorRGB(index, ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPackedM(c)));
}
D3DCOLOR DxScriptPrimitiveObject2D::GetVertexColor(size_t index) {
	RenderObjectTLX* obj = GetObjectPointer();
	return obj->GetVertexColor(index);
}
D3DXVECTOR3 DxScriptPrimitiveObject2D::GetVertexPosition(size_t index) {
	D3DXVECTOR3 res(0, 0, 0);
	if (!IsValidVertexIndex(index)) return res;
	RenderObjectTLX* obj = GetObjectPointer();
	VERTEX_TLX* vert = obj->GetVertex(index);

	constexpr float bias = 0.5f;
	res.x = vert->position.x + bias;
	res.y = vert->position.y + bias;
	res.z = 0;

	return res;
}

void DxScriptPrimitiveObject2D::SetPermitCamera(bool bPermit) {
	RenderObjectTLX* obj = GetObjectPointer();
	obj->SetPermitCamera(bPermit);
}

//****************************************************************************
//DxScriptSpriteObject2D
//****************************************************************************
DxScriptSpriteObject2D::DxScriptSpriteObject2D() {
	typeObject_ = TypeObject::Sprite2D;
	objRender_ = std::make_shared<Sprite2D>();
	objRender_->SetDxObjectReference(this);
	_AttachSpriteToAnimation(objRender_);
}

void DxScriptSpriteObject2D::Render() {
	Animate();
	DxScriptPrimitiveObject2D::Render();
}

void DxScriptSpriteObject2D::Copy(DxScriptSpriteObject2D* src) {
	priRender_ = src->priRender_;
	bZWrite_ = src->bZWrite_;
	bZTest_ = src->bZTest_;
	modeCulling_ = src->modeCulling_;
	bVisible_ = src->bVisible_;
	manager_ = src->manager_;
	position_ = src->position_;
	angle_ = src->angle_;
	scale_ = src->scale_;
	typeBlend_ = src->typeBlend_;

	Sprite2D* destSprite2D = (Sprite2D*)objRender_.get();
	Sprite2D* srcSprite2D = (Sprite2D*)src->objRender_.get();
	destSprite2D->Copy(srcSprite2D);
}

//****************************************************************************
//DxScriptSpriteListObject2D
//****************************************************************************
DxScriptSpriteListObject2D::DxScriptSpriteListObject2D() {
	typeObject_ = TypeObject::SpriteList2D;
	objRender_ = std::make_shared<SpriteList2D>();
	objRender_->SetDxObjectReference(this);
}

void DxScriptSpriteListObject2D::CleanUp() {
	GetSpritePointer()->CleanUp();
}

void DxScriptSpriteListObject2D::SetColor(int r, int g, int b) {
	D3DCOLOR color = GetSpritePointer()->GetColor();

	__m128i c = Vectorize::Set(color_ >> 24, r, g, b);
	color_ = ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPacked(c));

	GetSpritePointer()->SetColor(color_);
}
void DxScriptSpriteListObject2D::SetAlpha(int alpha) {
	D3DCOLOR color = GetSpritePointer()->GetColor();
	ColorAccess::ClampColor(alpha);
	color_ = (color & 0x00ffffff) | ((byte)alpha << 24);

	GetSpritePointer()->SetColor(color_);
}

void DxScriptSpriteListObject2D::AddVertex() {
	SpriteList2D* obj = GetSpritePointer();
	obj->SetPosition(position_);
	obj->SetAngle(angle_);
	obj->SetScale(scale_);
	obj->AddVertex(angX_, angY_, angZ_);
}
void DxScriptSpriteListObject2D::CloseVertex() {
	SpriteList2D* obj = GetSpritePointer();
	obj->CloseVertex();

	position_ = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	angle_ = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	scale_ = D3DXVECTOR3(1.0f, 1.0f, 1.0f);

	angX_ = D3DXVECTOR2(1, 0);
	angY_ = D3DXVECTOR2(1, 0);
	angZ_ = D3DXVECTOR2(1, 0);
}

//****************************************************************************
//DxScriptPrimitiveObject3D
//****************************************************************************
DxScriptPrimitiveObject3D::DxScriptPrimitiveObject3D() {
	typeObject_ = TypeObject::Primitive3D;
	objRender_ = std::make_shared<RenderObjectLX>();
	objRender_->SetDxObjectReference(this);
	bZWrite_ = false;
	bZTest_ = true;
	bFogEnable_ = true;
}

void DxScriptPrimitiveObject3D::Render() {
	if (RenderObjectLX* obj = GetObjectPointer()) {
		DirectGraphics* graphics = DirectGraphics::GetBase();
		bool bEnvFogEnable = graphics->IsFogEnable();
		SetRenderState();
		//obj->Render();
		obj->Render(angX_, angY_, angZ_);

		if (bEnvFogEnable)
			graphics->SetFogEnable(true);
	}
}
void DxScriptPrimitiveObject3D::SetRenderState() {
	if (DxScriptMeshObject* objMesh = dynamic_cast<DxScriptMeshObject*>(objRelative_.get())) {
		objRelative_->SetRenderState();

		int frameAnime = objMesh->GetAnimeFrame();
		const std::wstring& nameAnime = objMesh->GetAnimeName();
		shared_ptr<DxMesh> mesh = objMesh->GetMesh();
		shared_ptr<D3DXMATRIX> mat = std::make_shared<D3DXMATRIX>();
		*mat = mesh->GetAnimationMatrix(nameAnime, frameAnime);
		objRender_->SetRalativeMatrix(mat);
	}

	DirectGraphics* graphics = DirectGraphics::GetBase();
	RenderObjectLX* obj = GetObjectPointer();

	bool bEnvFogEnable = graphics->IsFogEnable();
	if (bEnvFogEnable)
		graphics->SetFogEnable(bFogEnable_);

	obj->SetPosition(position_);
	obj->SetAngle(angle_);
	obj->SetScale(scale_);
	obj->GetLighting()->Apply();
	graphics->SetZWriteEnable(bZWrite_);
	graphics->SetZBufferEnable(bZTest_);
	graphics->SetBlendMode(typeBlend_);
	graphics->SetCullingMode(modeCulling_);
	graphics->SetTextureFilter(filterMin_, filterMag_, filterMip_);
	obj->SetVertexShaderRendering(bVertexShaderMode_);
	obj->SetDisableMatrixTransformation(!bEnableMatrix_);
}

void DxScriptPrimitiveObject3D::SetColor(int r, int g, int b) {
	RenderObjectLX* obj = GetObjectPointer();
	__m128i c = Vectorize::Set(color_ >> 24, r, g, b);
	color_ = ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPacked(c));
	obj->SetColorRGB(color_);
}
void DxScriptPrimitiveObject3D::SetAlpha(int alpha) {
	RenderObjectLX* obj = GetObjectPointer();
	ColorAccess::ClampColor(alpha);
	obj->SetAlpha(alpha);
	color_ = (color_ & 0x00ffffff) | ((byte)alpha << 24);
}

bool DxScriptPrimitiveObject3D::IsValidVertexIndex(size_t index) {
	return index >= 0 && index < GetObjectPointer()->GetVertexCount();
}
void DxScriptPrimitiveObject3D::SetVertexPosition(size_t index, float x, float y, float z) {
	RenderObjectLX* obj = GetObjectPointer();
	obj->SetVertexPosition(index, x, y, z);
}
void DxScriptPrimitiveObject3D::SetVertexUV(size_t index, float u, float v) {
	RenderObjectLX* obj = GetObjectPointer();
	obj->SetVertexUV(index, u, v);
}
void DxScriptPrimitiveObject3D::SetVertexAlpha(size_t index, int alpha) {
	RenderObjectLX* obj = GetObjectPointer();
	ColorAccess::ClampColor(alpha);
	obj->SetVertexAlpha(index, alpha);
}
void DxScriptPrimitiveObject3D::SetVertexColor(size_t index, int r, int g, int b) {
	if (!IsValidVertexIndex(index)) return;
	RenderObjectLX* obj = GetObjectPointer();
	__m128i c = Vectorize::Set(0, r, g, b);
	obj->SetVertexColorRGB(index, ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPackedM(c)));
}
D3DCOLOR DxScriptPrimitiveObject3D::GetVertexColor(size_t index) {
	RenderObjectLX* obj = GetObjectPointer();
	return obj->GetVertexColor(index);
}
D3DXVECTOR3 DxScriptPrimitiveObject3D::GetVertexPosition(size_t index) {
	D3DXVECTOR3 res(0, 0, 0);
	if (!IsValidVertexIndex(index)) return res;
	RenderObjectLX* obj = GetObjectPointer();
	VERTEX_LX* vert = obj->GetVertex(index);

	res.x = vert->position.x;
	res.y = vert->position.y;
	res.z = vert->position.z;

	return res;
}

//****************************************************************************
//DxScriptSpriteObject3D
//****************************************************************************
DxScriptSpriteObject3D::DxScriptSpriteObject3D() {
	typeObject_ = TypeObject::Sprite3D;
	objRender_ = std::make_shared<Sprite3D>();
	objRender_->SetDxObjectReference(this);
	_AttachSpriteToAnimation(objRender_);
}

void DxScriptSpriteObject3D::Render() {
	Animate();
	DxScriptPrimitiveObject3D::Render();
}

//****************************************************************************
//DxScriptTrajectoryObject3D
//****************************************************************************
DxScriptTrajectoryObject3D::DxScriptTrajectoryObject3D() {
	typeObject_ = TypeObject::Trajectory3D;
	objRender_ = std::make_shared<TrajectoryObject3D>();
	color_ = 0xffffffff;
}

void DxScriptTrajectoryObject3D::Work() {
	if (TrajectoryObject3D* obj = GetObjectPointer()) {
		if (DxScriptMeshObject* objMesh = dynamic_cast<DxScriptMeshObject*>(objRelative_.get())) {
			objRelative_->SetRenderState();
			int frameAnime = objMesh->GetAnimeFrame();
			const std::wstring& nameAnime = objMesh->GetAnimeName();
			shared_ptr<DxMesh> mesh = objMesh->GetMesh();
			D3DXMATRIX matAnime = mesh->GetAnimationMatrix(nameAnime, frameAnime);

			TrajectoryObject3D* objRender = GetObjectPointer();
			objRender->AddPoint(matAnime);
		}

		obj->Work();
	}
}
void DxScriptTrajectoryObject3D::Render() {
	if (TrajectoryObject3D* obj = GetObjectPointer()) {
		SetRenderState();
		//obj->Render();
		obj->Render(angX_, angY_, angZ_);
	}
}
void DxScriptTrajectoryObject3D::SetRenderState() {
	DirectGraphics* graphics = DirectGraphics::GetBase();
	TrajectoryObject3D* obj = GetObjectPointer();
	graphics->SetLightingEnable(false);
	graphics->SetZWriteEnable(bZWrite_);
	graphics->SetZBufferEnable(bZTest_);
	graphics->SetBlendMode(typeBlend_);
	graphics->SetCullingMode(modeCulling_);
	graphics->SetTextureFilter(filterMin_, filterMag_, filterMip_);
	obj->SetVertexShaderRendering(bVertexShaderMode_);
	obj->SetDisableMatrixTransformation(!bEnableMatrix_);
}

void DxScriptTrajectoryObject3D::SetColor(int r, int g, int b) {
	DirectGraphics* graphics = DirectGraphics::GetBase();
	TrajectoryObject3D* obj = GetObjectPointer();

	__m128i c = Vectorize::Set(color_ >> 24, r, g, b);
	color_ = ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPacked(c));

	obj->SetColor(color_);
}

//****************************************************************************
//DxScriptParticleListObject2D
//****************************************************************************
DxScriptParticleListObject2D::DxScriptParticleListObject2D() {
	typeObject_ = TypeObject::ParticleList2D;
	objRender_ = std::make_shared<ParticleRenderer2D>();
	objRender_->SetDxObjectReference(this);
}

void DxScriptParticleListObject2D::Render() {
	ParticleRenderer2D* obj = GetParticlePointer();

	//Save fog state
	DirectGraphics* graphics = DirectGraphics::GetBase();
	DWORD bEnableFog = FALSE;
	graphics->GetDevice()->GetRenderState(D3DRS_FOGENABLE, &bEnableFog);
	if (bEnableFog)
		graphics->SetFogEnable(false);

	SetRenderState();
	obj->Render();

	if (bEnableFog)
		graphics->SetFogEnable(true);
}
void DxScriptParticleListObject2D::SetRenderState() {
	DirectGraphics* graphics = DirectGraphics::GetBase();
	ParticleRenderer2D* obj = GetParticlePointer();
	obj->SetPosition(position_);
	obj->SetAngle(angle_);
	obj->SetScale(scale_);
	graphics->SetLightingEnable(false);
	graphics->SetZWriteEnable(false);
	graphics->SetZBufferEnable(false);
	graphics->SetBlendMode(typeBlend_);
	graphics->SetCullingMode(modeCulling_);
	graphics->SetTextureFilter(filterMin_, filterMag_, filterMip_);
}
void DxScriptParticleListObject2D::CleanUp() {
	ParticleRenderer2D* obj = GetParticlePointer();
	if (obj->GetAutoClearInstance())
		obj->ClearInstance();
}

//****************************************************************************
//DxScriptParticleListObject3D
//****************************************************************************
DxScriptParticleListObject3D::DxScriptParticleListObject3D() {
	typeObject_ = TypeObject::ParticleList3D;
	objRender_ = std::make_shared<ParticleRenderer3D>();
	objRender_->SetDxObjectReference(this);
}

void DxScriptParticleListObject3D::Render() {
	ParticleRenderer3D* obj = GetParticlePointer();
	DirectGraphics* graphics = DirectGraphics::GetBase();

	bool bEnvFogEnable = graphics->IsFogEnable();

	SetRenderState();
	obj->Render();

	graphics->SetFogEnable(bEnvFogEnable);
}
void DxScriptParticleListObject3D::SetRenderState() {
	DirectGraphics* graphics = DirectGraphics::GetBase();

	bool bEnvFogEnable = graphics->IsFogEnable();
	if (bEnvFogEnable)
		graphics->SetFogEnable(bFogEnable_);

	ParticleRenderer3D* obj = GetParticlePointer();
	obj->SetPosition(position_);
	obj->SetAngle(angle_);
	obj->SetScale(scale_);
	//obj->GetLighting()->Apply();
	graphics->SetLightingEnable(false);
	graphics->SetZWriteEnable(bZWrite_);
	graphics->SetZBufferEnable(bZTest_);
	graphics->SetBlendMode(typeBlend_);
	graphics->SetCullingMode(modeCulling_);
	graphics->SetTextureFilter(filterMin_, filterMag_, filterMip_);
}
void DxScriptParticleListObject3D::CleanUp() {
	ParticleRenderer3D* obj = GetParticlePointer();
	if (obj->GetAutoClearInstance())
		obj->ClearInstance();
}

//****************************************************************************
//DxScriptMeshObject
//****************************************************************************
DxScriptMeshObject::DxScriptMeshObject() {
	typeObject_ = TypeObject::Mesh;
	bZWrite_ = true;
	bZTest_ = true;
	bFogEnable_ = true;
	time_ = 0;
	anime_ = L"";
	color_ = 0xffffffff;
	bCoordinate2D_ = false;

	angX_ = D3DXVECTOR2(1, 0);
	angY_ = D3DXVECTOR2(1, 0);
	angZ_ = D3DXVECTOR2(1, 0);
}

void DxScriptMeshObject::Render() {
	if (mesh_ == nullptr) return;
	DirectGraphics* graphics = DirectGraphics::GetBase();
	bool bEnvFogEnable = graphics->IsFogEnable();

	SetRenderState();
	mesh_->Render(anime_, time_, angX_, angY_, angZ_);

	graphics->SetFogEnable(bEnvFogEnable);
}
void DxScriptMeshObject::SetRenderState() {
	DirectGraphics* graphics = DirectGraphics::GetBase();

	if (graphics->IsFogEnable())
		graphics->SetFogEnable(bFogEnable_);

	mesh_->SetPosition(position_);
	mesh_->SetAngle(angle_);
	mesh_->SetScale(scale_);
	mesh_->SetColor(color_);
	mesh_->SetCoordinate2D(bCoordinate2D_);

	mesh_->GetLighting()->Apply();
	graphics->SetZWriteEnable(bZWrite_);
	graphics->SetZBufferEnable(bZTest_);
	
	graphics->SetBlendMode(typeBlend_);
	graphics->SetCullingMode(modeCulling_);

	graphics->SetTextureFilter(filterMin_, filterMag_, filterMip_);
	mesh_->SetVertexShaderRendering(bVertexShaderMode_);
	//mesh_->SetDisableMatrixTransformation(!bEnableMatrix_);
}

void DxScriptMeshObject::SetColor(int r, int g, int b) {
	__m128i c = Vectorize::Set(color_ >> 24, r, g, b);
	color_ = ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPacked(c));
}
void DxScriptMeshObject::SetAlpha(int alpha) {
	ColorAccess::ClampColor(alpha);
	color_ = (color_ & 0x00ffffff) | ((byte)alpha << 24);
}
shared_ptr<Shader> DxScriptMeshObject::GetShader() {
	if (mesh_ == nullptr) return nullptr;
	return mesh_->GetShader();
}
void DxScriptMeshObject::SetShader(shared_ptr<Shader> shader) {
	if (mesh_ == nullptr) return;
	mesh_->SetShader(shader);
}

void DxScriptMeshObject::SetAngleX(float x) {
	x = gstd::Math::DegreeToRadian(x);
	if (angle_.x != x) {
		angle_.x = x;
		angX_ = D3DXVECTOR2(cosf(-x), sinf(-x));
	}
}
void DxScriptMeshObject::SetAngleY(float y) {
	y = gstd::Math::DegreeToRadian(y);
	if (angle_.y != y) {
		angle_.y = y;
		angY_ = D3DXVECTOR2(cosf(-y), sinf(-y));
	}
}
void DxScriptMeshObject::SetAngleZ(float z) {
	z = gstd::Math::DegreeToRadian(z);
	if (angle_.z != z) {
		angle_.z = z;
		angZ_ = D3DXVECTOR2(cosf(-z), sinf(-z));
	}
}

//****************************************************************************
//DxScriptTextObject
//****************************************************************************
DxScriptTextObject::DxScriptTextObject() {
	typeObject_ = TypeObject::Text;
	change_ = CHANGE_ALL;
	bAutoCenter_ = true;
	center_ = D3DXVECTOR2(0, 0);

	angX_ = D3DXVECTOR2(1, 0);
	angY_ = D3DXVECTOR2(1, 0);
	angZ_ = D3DXVECTOR2(1, 0);
}
void DxScriptTextObject::Render() {
	//Save fog state, text objects don't fucking get fogged
	DirectGraphics* graphics = DirectGraphics::GetBase();
	DWORD bEnableFog = FALSE;
	graphics->GetDevice()->GetRenderState(D3DRS_FOGENABLE, &bEnableFog);
	if (bEnableFog)
		graphics->SetFogEnable(false);

	SetRenderState();
	text_.SetPosition(position_.x, position_.y);
	_UpdateRenderer();

	objRender_->SetPosition(position_.x, position_.y);
	objRender_->SetAngle(angle_);
	objRender_->SetScale(scale_);
	objRender_->SetVertexColor(text_.GetVertexColor());
	objRender_->SetTransCenter(center_);
	objRender_->SetAutoCenter(bAutoCenter_);
	objRender_->Render(angX_, angY_, angZ_);

	if (bEnableFog)
		graphics->SetFogEnable(true);
}
void DxScriptTextObject::SetRenderState() {
	DirectGraphics* graphics = DirectGraphics::GetBase();
	graphics->SetLightingEnable(false);
	graphics->SetZWriteEnable(false);
	graphics->SetZBufferEnable(false);
	graphics->SetBlendMode(typeBlend_);
	graphics->SetCullingMode(D3DCULL_NONE);
	graphics->SetTextureFilter(filterMin_, filterMag_, D3DTEXF_NONE);
}
void DxScriptTextObject::_UpdateRenderer() {
	if (change_ & CHANGE_INFO)
		textInfo_ = text_.GetTextInfo();
	if (change_ & CHANGE_RENDERER)
		objRender_ = text_.CreateRenderObject(textInfo_);
	change_ = 0;
}
void DxScriptTextObject::SetCharset(BYTE set) {
	/*
	switch (set) {
	case ANSI_CHARSET:
	case HANGUL_CHARSET:
	case ARABIC_CHARSET:
	case HEBREW_CHARSET:
	case THAI_CHARSET:
	case SHIFTJIS_CHARSET:
		break;
	default:
		set = ANSI_CHARSET;
		break;
	}
	*/
	text_.SetFontCharset(set); 
	change_ = CHANGE_ALL;
}
void DxScriptTextObject::SetText(const std::wstring& text) {
	size_t newHash = std::hash<std::wstring>{}(text);
	if (newHash == text_.GetTextHash()) return;

	text_.SetTextHash(newHash);
	text_.SetText(text); 

	change_ = CHANGE_ALL;
}
std::vector<size_t> DxScriptTextObject::GetTextCountCU() {
	_UpdateRenderer();
	size_t lineCount = textInfo_->GetLineCount();
	std::vector<size_t> listCount;
	for (size_t iLine = 0; iLine < lineCount; ++iLine) {
		shared_ptr<DxTextLine> textLine = textInfo_->GetTextLine(iLine);
		size_t count = textLine->GetTextCodeCount();
		listCount.push_back(count);
	}
	return listCount;
}

void DxScriptTextObject::SetAlpha(int alpha) {
	D3DCOLOR color = text_.GetVertexColor();
	ColorAccess::ClampColor(alpha);
	color_ = (color & 0x00ffffff) | ((byte)alpha << 24);
	SetVertexColor(color_);
}
void DxScriptTextObject::SetColor(int r, int g, int b) {
	D3DCOLOR color = text_.GetVertexColor();
	__m128i c = Vectorize::Set(color >> 24, r, g, b);
	color_ = ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPacked(c));
	SetVertexColor(color_);
}

LONG DxScriptTextObject::GetTotalWidth() {
	if (change_ & CHANGE_INFO) {
		textInfo_ = text_.GetTextInfo();
		change_ &= (~CHANGE_INFO);
	}
	return textInfo_->GetTotalWidth();
}
LONG DxScriptTextObject::GetTotalHeight() {
	if (change_ & CHANGE_INFO) {
		textInfo_ = text_.GetTextInfo();
		change_ &= (~CHANGE_INFO);
	}
	return textInfo_->GetTotalHeight();
}

void DxScriptTextObject::SetShader(shared_ptr<Shader> shader) {
	text_.SetShader(shader);
	change_ = CHANGE_ALL;
}

void DxScriptTextObject::SetAngleX(float x) {
	x = gstd::Math::DegreeToRadian(x);
	if (angle_.x != x) {
		angle_.x = x;
		angX_ = D3DXVECTOR2(cosf(-x), sinf(-x));
	}
}
void DxScriptTextObject::SetAngleY(float y) {
	y = gstd::Math::DegreeToRadian(y);
	if (angle_.y != y) {
		angle_.y = y;
		angY_ = D3DXVECTOR2(cosf(-y), sinf(-y));
	}
}
void DxScriptTextObject::SetAngleZ(float z) {
	z = gstd::Math::DegreeToRadian(z);
	if (angle_.z != z) {
		angle_.z = z;
		angZ_ = D3DXVECTOR2(cosf(-z), sinf(-z));
	}
}

//****************************************************************************
//DxSoundObject
//****************************************************************************
DxSoundObject::DxSoundObject() {
	typeObject_ = TypeObject::Sound;
}
DxSoundObject::~DxSoundObject() {
	if (player_)
		player_->Delete();
}

bool DxSoundObject::Load(const std::wstring& path) {
	DirectSoundManager* manager = DirectSoundManager::GetBase();

	{
		Lock lock(manager->GetLock());

		shared_ptr<SoundSourceData> newSource = manager->GetSoundSource(path, true);
		bool bFoundCached = false;

		//A very ugly hack
		player_ = nullptr;
		if (newSource) {
			for (auto itr = mapCachedPlayers_.begin(); itr != mapCachedPlayers_.end();) {
				SoundSourceData* pSource = itr->first;
				weak_ptr<SoundPlayer> pWeakPlayer = itr->second;
				if (pWeakPlayer.expired()) {
					itr = mapCachedPlayers_.erase(itr);
				}
				else {
					if (pSource == newSource.get()) {
						player_ = pWeakPlayer.lock();
						bFoundCached = true;
						break;
					}
					++itr;
				}
			}
		}

		if (player_ == nullptr) {
			player_ = manager->CreatePlayer(newSource);
		}

		if (player_ && !bFoundCached) {
			mapCachedPlayers_[newSource.get()] = player_;
		}
	}
	return player_ != nullptr;
}
void DxSoundObject::Play() {
	if (player_)
		player_->Play();
}

//****************************************************************************
//DxFileObject
//****************************************************************************
DxFileObject::DxFileObject() {
	bWritable_ = false;
}
DxFileObject::~DxFileObject() {
	Close();
}

bool DxFileObject::OpenR(const std::wstring& path) {
	std::wstring cPath = path;
	cPath = PathProperty::Canonicalize(cPath);
	cPath = PathProperty::ReplaceYenToSlash(cPath);

	file_ = shared_ptr<File>(new File(cPath));
	bool res = file_->Open();
	if (!res) {
		file_ = nullptr;
		std::wstring err = StringUtility::Format(L"DxFileObject: Failed to open file. [%s]\r\n\tSystem: %s",
			path.c_str(), File::GetLastError().c_str());
		Logger::WriteTop(err);
	}
	return res;
}
bool DxFileObject::OpenR(shared_ptr<gstd::FileReader> reader) {
	file_ = nullptr;
	reader_ = reader;
	return true;
}
bool DxFileObject::OpenRW(const std::wstring& path) {
	std::wstring cPath = path;
	cPath = PathProperty::Canonicalize(cPath);
	cPath = PathProperty::ReplaceYenToSlash(cPath);

	std::wstring dir = PathProperty::GetFileDirectory(cPath);
	bool bDir = File::CreateFileDirectory(dir);
	if (!bDir) {
		Logger::WriteTop(StringUtility::Format("DxFileObject: Failed to create the directory. [%s]", path.c_str()));
		return false;
	}

	//Security; to prevent scripts from being able to access external files
	const std::wstring& dirModule = PathProperty::GetModuleDirectory();
	if (dir.find(dirModule) == std::wstring::npos) {
		Logger::WriteTop(StringUtility::Format("DxFileObject: OpenW cannot open external files. [%s]", path.c_str()));
		return false;
	}

	file_ = shared_ptr<File>(new File(cPath));
	bool res = file_->Open(File::WRITE);
	if (!res) {
		file_ = nullptr;
		std::wstring err = StringUtility::Format(L"DxFileObject: Failed to open file. [%s]\r\n\tSystem: %s", 
			path.c_str(), File::GetLastError().c_str());
		Logger::WriteTop(err);
	}
	return res;
}
void DxFileObject::Close() {
	if (file_) {
		file_->Close();
		file_ = nullptr;
	}
}

//****************************************************************************
//DxTextFileObject
//****************************************************************************
DxTextFileObject::DxTextFileObject() {
	typeObject_ = TypeObject::FileText;
	encoding_ = Encoding::UTF16LE;
	bomSize_ = 2U;
	memcpy(bomHead_, Encoding::BOM_UTF16LE, 2);
	bytePerChar_ = 2U;
}
DxTextFileObject::~DxTextFileObject() {
}

bool DxTextFileObject::OpenR(const std::wstring& path) {
	listLine_.clear();
	bool res = DxFileObject::OpenR(path);
	if (!res) return false;

	size_t size = file_->GetSize();
	if (size == 0) return true;

	std::vector<char> text;
	text.resize(size);
	file_->Read(&text[0], size);

	return _ParseLines(text);
}
bool DxTextFileObject::OpenR(shared_ptr<gstd::FileReader> reader) {
	listLine_.clear();
	reader_ = reader;

	size_t size = reader->GetFileSize();
	if (size == 0) return true;

	std::vector<char> text;
	text.resize(size);
	reader_->SetFilePointerBegin();
	reader_->Read(&text[0], size);

	return _ParseLines(text);
}
bool DxTextFileObject::OpenRW(const std::wstring& path) {
	bool res = DxFileObject::OpenRW(path);
	if (!res) return false;

	size_t size = file_->GetSize();
	if (size == 0) return true;

	std::vector<char> text;
	text.resize(size);
	file_->SetFilePointerBegin();
	file_->Read(&text[0], size);

	return _ParseLines(text);
}

bool DxTextFileObject::_ParseLines(std::vector<char>& src) {
	listLine_.clear();
	encoding_ = Encoding::Detect(src.data(), src.size());
	bomSize_ = 0;
	bytePerChar_ = 1;
	ZeroMemory(lineEnding_, sizeof(lineEnding_));
	lineEndingSize_ = 0;

	switch (encoding_) {
	case Encoding::UTF8BOM:
		bomSize_ = 3;
		memcpy(bomHead_, Encoding::BOM_UTF8, 3);
		break;
	case Encoding::UTF16LE:
		bomSize_ = 2;
		bytePerChar_ = 2;
		memcpy(bomHead_, Encoding::BOM_UTF16LE, 2);
		break;
	case Encoding::UTF16BE:
		bomSize_ = 2;
		bytePerChar_ = 2;
		memcpy(bomHead_, Encoding::BOM_UTF16BE, 2);
		break;
	}

	if (bomSize_ > src.size()) return true;

	try {
		if (bytePerChar_ == 1U) {			//UTF-8
			std::vector<char> tmp;

			auto itr = src.begin() + bomSize_;
			for (; itr != src.end();) {
				char ch = *itr;
				if (ch == '\n') {
					listLine_.push_back(tmp);
					tmp.clear();
				}
				else {
					tmp.push_back(ch);
				}
				++itr;
			}
			if (tmp.size() > 0) listLine_.push_back(tmp);

			for (auto& iLineVec : listLine_) {
				if (iLineVec.size() > 0) {
					if (lineEndingSize_ == 0) {
						if (iLineVec.back() == '\r') {
							memcpy(lineEnding_, "\r\n", 2);
							lineEndingSize_ = 2;
						}
						else {
							memcpy(lineEnding_, "\n", 1);
							lineEndingSize_ = 1;
						}
					}

					while (iLineVec.back() == '\r') {
						iLineVec.pop_back();
						if (iLineVec.empty()) break;
					}
				}
			}
		}
		else if (bytePerChar_ == 2U) {		//UTF-16
			bool bLittleEndian = encoding_ == Encoding::UTF16LE;

			//CR/LF base for comparisons
			wchar_t CH_CR = L'\r';
			wchar_t CH_LF = L'\n';
			if (!bLittleEndian) {	//Big Endian, swap bytes
				ByteOrder::Reverse(&CH_CR, 2);
				ByteOrder::Reverse(&CH_LF, 2);
			}

			auto push_line = [&](std::vector<char>& vecLine) {
				if (vecLine.size() & 0b1) vecLine.pop_back();
				listLine_.push_back(vecLine);
			};

			std::vector<char> tmp;

			auto itr = src.begin() + bomSize_;
			for (; itr != src.end();) {
				wchar_t* wch = (wchar_t*)itr._Ptr;
				if (*wch == CH_LF) {
					push_line(tmp);
					tmp.clear();
				}
				else {
					tmp.push_back(((char*)wch)[0]);
					tmp.push_back(((char*)wch)[1]);
				}
				itr += 2;
			}
			if (tmp.size() > 0) push_line(tmp);

			for (auto& iLineVec : listLine_) {
				if (iLineVec.size() > 0) {
					wchar_t* pWCh = (wchar_t*)((&iLineVec.back()) - 1);

					if (lineEndingSize_ == 0) {
						if (*pWCh == CH_CR) {
							memcpy(&lineEnding_[0], &CH_CR, 2);
							memcpy(&lineEnding_[2], &CH_LF, 2);
							lineEndingSize_ = 4;
						}
						else {
							memcpy(&lineEnding_[0], &CH_LF, 2);
							lineEndingSize_ = 2;
						}
					}

					while (*pWCh == CH_CR) {
						iLineVec.pop_back();
						iLineVec.pop_back();
						if (iLineVec.empty()) break;
						pWCh = (wchar_t*)((&iLineVec.back()) - 1);
					}

					if (!bLittleEndian) {
						//Turn Big Endian into Little Endian
						for (auto itr = iLineVec.begin(); itr != iLineVec.end(); itr += 2) {
							std::swap(*itr, *(itr + 1));
						}
					}
				}
			}
		}
	}
	catch (...) {
		return false;
	}
	return true;
}
bool DxTextFileObject::Store() {
	if (!bWritable_ || file_ == nullptr) return false;

	file_->Seek(0, std::ios::beg, File::WRITE);

	if (bomSize_ > 0) file_->Write(bomHead_, bomSize_);

	if (lineEndingSize_ == 0) {
		if (bytePerChar_ == 1) {
			lineEnding_[0] = '\n';
			lineEndingSize_ = 1;
		}
		else {
			memcpy(lineEnding_, encoding_ == Encoding::UTF16LE ? "\n\0" : "\0\n", 2);
			lineEndingSize_ = 2;
		}
	}

	for (size_t iLine = 0; iLine < listLine_.size(); ++iLine) {
		std::vector<char>& str = listLine_[iLine];

		if (encoding_ == Encoding::UTF16LE || bytePerChar_ == 1) {
			if (str.size() > 0)
				file_->Write(&str[0], str.size());
		}
		else if (encoding_ == Encoding::UTF16BE) {
			if (str.size() > 0) {
				std::vector<char> vecTmp = str;
				for (auto itr = vecTmp.begin(); itr != vecTmp.end(); itr += 2) {
					std::swap(*itr, *(itr + 1));
				}
				file_->Write(&vecTmp[0], vecTmp.size());
			}
		}

		if (iLine < listLine_.size() - 1)
			file_->Write(lineEnding_, lineEndingSize_);
	}
	return true;
}

std::string DxTextFileObject::GetLineAsString(size_t line) {
	line--; //Line number begins at 1
	if (line >= listLine_.size()) return "";

	std::string res;
	std::vector<char>& src = listLine_[line];
	if (src.size() > 0) {
		if (bytePerChar_ == 1) {
			res.resize(src.size());
			memcpy(res.data(), src.data(), src.size());
		}
		else if (bytePerChar_ == 2) {
			res = StringUtility::ConvertWideToMulti(src);
		}
	}
	return res;
}
std::wstring DxTextFileObject::GetLineAsWString(size_t line) {
	line--;		//Line number begins at 1
	if (line >= listLine_.size()) return L"";

	std::wstring res = L"";
	std::vector<char>& src = listLine_[line];
	if (src.size() > 0) {
		if (bytePerChar_ == 2) {
			res.resize(src.size() / 2U);
			memcpy(res.data(), src.data(), res.size() * sizeof(wchar_t));
		}
		else if (bytePerChar_ == 1) {
			res = StringUtility::ConvertMultiToWide(src);
		}
	}
	return res;
}
void DxTextFileObject::SetLineAsString(const std::string& text, size_t line) {
	line--;		//Line number begins at 1
	if (line >= listLine_.size()) return;

	std::vector<char> newLine;
	if (text.size() > 0) {
		if (bytePerChar_ == 1) {
			newLine.resize(text.size());
			memcpy(&newLine[0], &text[0], newLine.size());
		}
		else if (bytePerChar_ == 2) {
			std::wstring _res = StringUtility::ConvertMultiToWide(text);
			newLine.resize(_res.size() * 2U);
			memcpy(&newLine[0], &_res[0], newLine.size());
		}
	}
	listLine_[line] = newLine;
}
void DxTextFileObject::SetLineAsWString(const std::wstring& text, size_t line) {
	line--; //Line number begins at 1
	if (line >= listLine_.size()) return;

	std::vector<char> newLine;
	if (text.size() > 0) {
		if (bytePerChar_ == 2) {
			newLine.resize(text.size() * 2U);
			memcpy(&newLine[0], &text[0], newLine.size());
		}
		else if (bytePerChar_ == 1) {
			std::string _res = StringUtility::ConvertWideToMulti(text);
			newLine.resize(_res.size());
			memcpy(&newLine[0], &_res[0], newLine.size());
		}
	}
	listLine_[line] = newLine;
}

void DxTextFileObject::_AddLine(const char* pChar, size_t count) {
	std::vector<char> newLine;
	if (count > 0) {
		newLine.resize(count);
		memcpy(&newLine[0], pChar, count);
	}
	listLine_.push_back(newLine);
}
void DxTextFileObject::AddLine(const std::string& line) {
	if (bytePerChar_ == 1) _AddLine(&line[0], line.size() * sizeof(char));
	else if (bytePerChar_ == 2) {
		std::wstring str = StringUtility::ConvertMultiToWide(line);
		_AddLine((char*)&str[0], str.size() * sizeof(wchar_t));
	}
}
void DxTextFileObject::AddLine(const std::wstring& line) {
	if (bytePerChar_ == 2) _AddLine((const char*)&line[0], line.size() * sizeof(wchar_t));
	else if (bytePerChar_ == 1) {
		std::string str = StringUtility::ConvertWideToMulti(line);
		_AddLine(&str[0], str.size() * sizeof(char));
	}
}

//****************************************************************************
//DxBinaryFileObject
//****************************************************************************
DxBinaryFileObject::DxBinaryFileObject() {
	typeObject_ = TypeObject::FileBinary;
	byteOrder_ = ByteOrder::ENDIAN_LITTLE;
	codePage_ = CP_ACP;
}
DxBinaryFileObject::~DxBinaryFileObject() {
	ptr_delete(buffer_);
}

bool DxBinaryFileObject::OpenR(const std::wstring& path) {
	bool res = DxFileObject::OpenR(path);
	if (!res) return false;

	size_t size = file_->GetSize();
	buffer_ = new ByteBuffer();
	buffer_->SetSize(size);

	file_->Read(buffer_->GetPointer(), size);

	return true;
}
bool DxBinaryFileObject::OpenR(shared_ptr<gstd::FileReader> reader) {
	reader_ = reader;

	auto srcBuffer = dynamic_cast<ManagedFileReader*>(reader.get())->GetBuffer();

	size_t size = reader->GetFileSize();
	buffer_ = new ByteBuffer();
	buffer_->Copy(*srcBuffer);
	
	return true;
}
bool DxBinaryFileObject::OpenRW(const std::wstring& path) {
	bool res = DxFileObject::OpenRW(path);
	if (!res) return false;

	size_t size = file_->GetSize();
	buffer_ = new ByteBuffer();
	buffer_->SetSize(size);

	file_->Read(buffer_->GetPointer(), size);

	return true;
}
bool DxBinaryFileObject::Store() {
	if (!bWritable_ || file_ == nullptr) return false;

	file_->SetFilePointerBegin(File::WRITE);
	file_->Write(buffer_->GetPointer(), buffer_->GetSize());

	return true;
}

bool DxBinaryFileObject::IsReadableSize(size_t size) {
	size_t pos = buffer_->GetOffset();
	bool res = pos + size <= buffer_->GetSize();
	return res;
}
DWORD DxBinaryFileObject::Read(LPVOID data, size_t size) {
	if (buffer_ == nullptr || data == nullptr || size == 0) {
		lastRead_ = 0;
	}
	else {
		lastRead_ = buffer_->Read(data, size);
	}
	return lastRead_;
}
DWORD DxBinaryFileObject::Write(LPVOID data, size_t size) {
	if (buffer_ == nullptr) return 0;
	return buffer_->Write(data, size);
}

//****************************************************************************
//DxScriptObjectManager
//****************************************************************************
DxScriptObjectManager::FogData DxScriptObjectManager::fogData_ = { false, 0xffffffff, 0, 0 };
DxScriptObjectManager::DxScriptObjectManager() {
	SetMaxObject(DEFAULT_CONTAINER_CAPACITY);
	SetRenderBucketCapacity(101);

	totalObjectCreateCount_ = 0U;

	listDeleteObject_.reserve(512U);
}
DxScriptObjectManager::~DxScriptObjectManager() {
}

bool DxScriptObjectManager::SetMaxObject(size_t size) {
	/*
	listUnusedIndex_.clear();
	for (size_t iObj = 0U; iObj < obj_.size(); ++iObj) {
		shared_ptr<DxScriptObjectBase> obj = obj_[iObj];
		if (obj == nullptr) listUnusedIndex_.push_back(iObj);
	}
	*/
	if (obj_.size() == 131072U) return false;
	size = std::min(size, 131072U);

	for (size_t iObj = obj_.size(); iObj < size; ++iObj)
		listUnusedIndex_.push_back(iObj);
	obj_.resize(size, nullptr);
	return true;
}
void DxScriptObjectManager::SetRenderBucketCapacity(size_t capacity) {
	listObjRender_.resize(capacity);
	listShader_.resize(capacity);
}

int DxScriptObjectManager::AddObject(ref_unsync_ptr<DxScriptObjectBase> obj, bool bActivate) {
	int res = DxScript::ID_INVALID;

	auto ExpandContainerCapacity = [&]() -> bool {
		size_t oldSize = obj_.size();
		bool res = SetMaxObject(oldSize * 2U);
		if (res) Logger::WriteTop(StringUtility::Format("DxScriptObjectManager: Object pool expansion. [%d->%d]",
			oldSize, obj_.size()));
		return res;
	};

	{
		do {
			if (listUnusedIndex_.size() == 0U) {
				if (!ExpandContainerCapacity()) break;
			}
			res = listUnusedIndex_.front();
			listUnusedIndex_.pop_front();
		} while (obj_[res]);

		if (res != DxScript::ID_INVALID) {
			obj_[res] = obj;

			if (bActivate) {
				obj->bActive_ = true;
				listActiveObject_.push_back(obj);
			}
			obj->idObject_ = res;
			obj->manager_ = this;

			++totalObjectCreateCount_;
		}
	}

	return res;
}

void DxScriptObjectManager::ActivateObject(int id, bool bActivate) {
	DxScriptObjectManager::ActivateObject(GetObject(id), bActivate);
}
void DxScriptObjectManager::ActivateObject(ref_unsync_ptr<DxScriptObjectBase> obj, bool bActivate) {
	if (obj == nullptr || obj->IsDeleted()) return;

	if (bActivate && !obj->IsActive()) {
		obj->bActive_ = true;
		listActiveObject_.push_back(obj);
	}
	else if (!bActivate) {
		obj->bActive_ = false;
	}
}

std::vector<int> DxScriptObjectManager::GetValidObjectIdentifier() {
	std::vector<int> res;
	for (size_t iObj = 0; iObj < obj_.size(); ++iObj) {
		if (obj_[iObj] == nullptr) continue;
		res.push_back(obj_[iObj]->idObject_);
	}
	return res;
}
DxScriptObjectBase* DxScriptObjectManager::GetObjectPointer(int id) {
	if (id < 0 || id >= obj_.size()) return nullptr;
	return obj_[id].get();
}

void DxScriptObjectManager::_DeleteObject(int id) {
	if (id < 0 || id >= obj_.size()) return;

	ref_unsync_ptr<DxScriptObjectBase> pObj = obj_[id];
	if (pObj == nullptr) return;

	pObj->bDelete_ = true;
	if (pObj->manager_)
		pObj->manager_->listUnusedIndex_.push_back(id);

	obj_[id] = nullptr;
	pObj->idObject_ = DxScript::ID_INVALID;
}

//DeleteObject marks object for actual deletion at the start of the next frame
void DxScriptObjectManager::DeleteObject(int id) {
	if (id < 0 || id >= obj_.size()) return;
	DeleteObject(obj_[id].get());
}
void DxScriptObjectManager::DeleteObject(ref_unsync_ptr<DxScriptObjectBase> obj) {
	DeleteObject(obj.get());
}
void DxScriptObjectManager::DeleteObject(DxScriptObjectBase* obj) {
	if (obj == nullptr) return;
	obj->bDelete_ = true;
	obj->bActive_ = false;
	listDeleteObject_.push_back(obj->idObject_);
}

void DxScriptObjectManager::ClearObject() {
	std::fill(obj_.begin(), obj_.end(), nullptr);
	listActiveObject_.clear();

	listUnusedIndex_.clear();
	for (size_t iObj = 0; iObj < obj_.size(); ++iObj) {
		listUnusedIndex_.push_back(iObj);
	}
}
void DxScriptObjectManager::DeleteObjectByScriptID(int64_t idScript) {
	if (idScript == ScriptClientBase::ID_SCRIPT_FREE) return;

	for (size_t iObj = 0; iObj < obj_.size(); ++iObj) {
		auto& pObj = obj_[iObj];
		if (pObj == nullptr) continue;
		if (pObj->GetScriptID() != idScript) continue;
		DeleteObject(pObj);
	}
}
void DxScriptObjectManager::OrphanObjectByScriptID(int64_t idScript) {
	if (idScript == ScriptClientBase::ID_SCRIPT_FREE) return;

	for (size_t iObj = 0; iObj < obj_.size(); ++iObj) {
		auto& pObj = obj_[iObj];
		if (pObj == nullptr) continue;
		if (pObj->GetScriptID() != idScript) continue;
		pObj->idScript_ = ScriptClientBase::ID_SCRIPT_FREE;
	}
}
std::vector<int> DxScriptObjectManager::GetObjectByScriptID(int64_t idScript) {
	std::vector<int> res;

	if (idScript != ScriptClientBase::ID_SCRIPT_FREE) {
		for (size_t iObj = 0; iObj < obj_.size(); ++iObj) {
			auto& pObj = obj_[iObj];
			if (pObj == nullptr) continue;
			if (pObj->GetScriptID() != idScript) continue;
			res.push_back(pObj->idObject_);
		}
	}
	return res;
}

shared_ptr<Shader> DxScriptObjectManager::GetShader(int index) {
	if (index < 0 || index >= listShader_.size()) return nullptr;
	shared_ptr<Shader> shader = listShader_[index];
	return shader;
}

void DxScriptObjectManager::WorkObject() {
	for (auto itr = listActiveObject_.begin(); itr != listActiveObject_.end();) {
		DxScriptObjectBase* obj = itr->get();
		if (obj == nullptr || obj->IsDeleted()) {
			itr = listActiveObject_.erase(itr);
			continue;
		}
		obj->Work();
		++(obj->frameExist_);
		++itr;
	}

	//Play cached sounds
	DirectSoundManager* soundManager = DirectSoundManager::GetBase();
	for (auto itrSound = mapReservedSound_.begin(); itrSound != mapReservedSound_.end(); ++itrSound) {
		shared_ptr<SoundPlayer> player = itrSound->second;
		player->Play();
	}
	mapReservedSound_.clear();
}
void DxScriptObjectManager::RenderObject() {
	PrepareRenderObject();

	DirectGraphics* graphics = DirectGraphics::GetBase();
	graphics->SetVertexFog(fogData_.enable, fogData_.color, fogData_.start, fogData_.end);

	for (size_t iPri = 0; iPri < listObjRender_.size(); ++iPri) {
		ID3DXEffect* effect = nullptr;

		UINT cPass = 1;
		if (shared_ptr<Shader> shader = listShader_[iPri]) {
			effect = shader->GetEffect();

			if (shader->LoadTechnique())
				shader->LoadParameter();
			effect->Begin(&cPass, 0);
		}

		RenderList& renderList = listObjRender_[iPri];

		for (UINT iPass = 0; iPass < cPass; ++iPass) {
			if (effect) effect->BeginPass(iPass);
			for (auto itr = renderList.begin(); itr != renderList.end(); ++itr) {
				(*itr)->Render();
			}
			if (effect) effect->EndPass();
		}
		renderList.Clear();

		if (effect) effect->End();
	}
}
void DxScriptObjectManager::CleanupObject() {
	for (auto& obj : listActiveObject_) {
		if (obj) obj->CleanUp();
	}

	for (int id : listDeleteObject_) {
		_DeleteObject(id);
	}
	listDeleteObject_.clear();
}

void DxScriptObjectManager::RenderList::Add(ref_unsync_ptr<DxScriptObjectBase>& ptr) {
	if (size >= list.size())
		list.push_back(ptr);
	else
		list[size++] = ptr;
}
void DxScriptObjectManager::RenderList::Clear() {
	size = 0U;
	std::fill(list.begin(), list.end(), nullptr);
}
void DxScriptObjectManager::PrepareRenderObject() {
	for (auto& obj : listActiveObject_) {
		if (obj == nullptr || obj->IsDeleted()) continue;
		//Some render objects don't use normal rendering, thus sorting isn't required for them
		if (!obj->HasNormalRendering()) continue;
		if (!obj->IsVisible()) continue;

		AddRenderObject(obj);
	}
}
void DxScriptObjectManager::AddRenderObject(ref_unsync_ptr<DxScriptObjectBase> obj) {
	size_t renderSize = listObjRender_.size();

	int tPri = obj->priRender_;
	if (tPri < 0) tPri = 0;
	else if (tPri > renderSize - 1) tPri = renderSize - 1;
	listObjRender_[tPri].Add(obj);
}
void DxScriptObjectManager::ClearRenderObject() {
	for (size_t iPri = 0; iPri < listObjRender_.size(); ++iPri) {
		listObjRender_[iPri].Clear();
	}
}

void DxScriptObjectManager::SetShader(shared_ptr<Shader> shader, int min, int max) {
	for (int iPri = min; iPri <= max; ++iPri) {
		if (iPri < 0 || iPri >= listShader_.size()) continue;
		listShader_[iPri] = shader;
	}
}
void DxScriptObjectManager::ResetShader() {
	ResetShader(0, listShader_.size());
}
void DxScriptObjectManager::ResetShader(int min, int max) {
	SetShader(nullptr, min, max);
}

void DxScriptObjectManager::ReserveSound(shared_ptr<SoundPlayer> player) {
	const std::wstring& path = player->GetPath();
	mapReservedSound_[path] = player;
}
void DxScriptObjectManager::DeleteReservedSound(shared_ptr<SoundPlayer> player) {
	const std::wstring& path = player->GetPath();
	mapReservedSound_.erase(path);
}
shared_ptr<SoundPlayer> DxScriptObjectManager::GetReservedSound(shared_ptr<SoundPlayer> player) {
	const std::wstring& path = player->GetPath();
	auto itr = mapReservedSound_.find(path);
	if (itr != mapReservedSound_.end())
		return itr->second;
	return nullptr;
}

void DxScriptObjectManager::SetFogParam(bool bEnable, D3DCOLOR fogColor, float start, float end) {
	fogData_.enable = bEnable;
	fogData_.color = fogColor;
	fogData_.start = start;
	fogData_.end = end;
}
