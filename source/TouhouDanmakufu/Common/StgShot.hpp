#pragma once

#include "../../GcLib/pch.h"

#include "StgCommon.hpp"
#include "StgIntersection.hpp"

class StgShotDataList;
class StgShotData;
class StgShotRenderer;
class StgShotObject;
//*******************************************************************
//StgShotManager
//*******************************************************************
class StgShotManager {
	friend class StgShotRenderer;
public:
	enum {
		DEL_TYPE_ALL,
		DEL_TYPE_SHOT,
		DEL_TYPE_CHILD,
		TO_TYPE_IMMEDIATE,
		TO_TYPE_FADE,
		TO_TYPE_ITEM,
	};

	enum {
		BIT_EV_DELETE_IMMEDIATE = 1,
		BIT_EV_DELETE_TO_ITEM,
		BIT_EV_DELETE_FADE,
		BIT_EV_DELETE_COUNT,
	};

	enum {
		SHOT_MAX = 16384,
	};
protected:
	StgStageController* stageController_;
	StgShotDataList* listPlayerShotData_;
	StgShotDataList* listEnemyShotData_;

	std::list<ref_unsync_ptr<StgShotObject>> listObj_;
	std::vector<std::pair<size_t, std::vector<StgShotObject*>>> listRenderQueue_;

	std::bitset<BIT_EV_DELETE_COUNT> listDeleteEventEnable_;

	DxRect<LONG> rcDeleteClip_;

	D3DTEXTUREFILTERTYPE filterMin_;
	D3DTEXTUREFILTERTYPE filterMag_;
	D3DTEXTUREFILTERTYPE filterMip_;

	ID3DXEffect* effectLayer_;
	D3DXHANDLE handleEffectWorld_;
public:
	StgShotManager(StgStageController* stageController);
	virtual ~StgShotManager();

	void Work();
	void Render(int targetPriority);
	void LoadRenderQueue();

	void RegistIntersectionTarget();

	void AddShot(ref_unsync_ptr<StgShotObject> obj);

	StgShotDataList* GetPlayerShotDataList() { return listPlayerShotData_; }
	StgShotDataList* GetEnemyShotDataList() { return listEnemyShotData_; }

	bool LoadPlayerShotData(const std::wstring& path, bool bReload = false);
	bool LoadEnemyShotData(const std::wstring& path, bool bReload = false);

	void SetShotDeleteClip(const DxRect<LONG>& clip) { rcDeleteClip_ = clip; }
	DxRect<LONG>* GetShotDeleteClip() { return &rcDeleteClip_; }

	void SetTextureFilter(D3DTEXTUREFILTERTYPE min, D3DTEXTUREFILTERTYPE mag, D3DTEXTUREFILTERTYPE mip) {
		filterMin_ = min;
		filterMag_ = mag;
		filterMip_ = mip;
	}

	size_t DeleteInCircle(int typeDelete, int typeTo, int typeOwner, int cx, int cy, int* radius);
	size_t DeleteInRegularPolygon(int typeDelete, int typeTo, int typeOwner, int cx, int cy, int* radius, int edges, double angle);
	std::vector<int> GetShotIdInCircle(int typeOwner, int cx, int cy, int* radius);
	std::vector<int> GetShotIdInRegularPolygon(int typeOwner, int cx, int cy, int* radius, int edges, double angle);
	size_t GetShotCount(int typeOwner);
	size_t GetShotCountAll() { return listObj_.size(); }

	void SetDeleteEventEnableByType(int type, bool bEnable);
	bool IsDeleteEventEnable(int bit) { return listDeleteEventEnable_[bit]; }
};

//*******************************************************************
//StgShotDataList
//*******************************************************************
class StgShotDataList {
public:
	enum {
		RENDER_TYPE_COUNT = 8,
	};
private:
	std::set<std::wstring> listReadPath_;
	std::vector<shared_ptr<Texture>> listTexture_;
	std::vector<std::vector<StgShotRenderer*>> listRenderer_;
	std::vector<StgShotData*> listData_;

	D3DCOLOR defaultDelayColor_;

	void _ScanShot(std::vector<StgShotData*>& listData, Scanner& scanner);
	static void _ScanAnimation(StgShotData*& shotData, Scanner& scanner);
public:
	StgShotDataList();
	virtual ~StgShotDataList();

	size_t GetTextureCount() { return listTexture_.size(); }
	shared_ptr<Texture> GetTexture(int index) { return listTexture_[index]; }
	StgShotRenderer* GetRenderer(int index, int typeRender) { return listRenderer_[typeRender][index]; }
	std::vector<StgShotRenderer*>& GetRendererList(int typeRender) { return listRenderer_[typeRender]; }

	StgShotData* GetData(int id) { return (id >= 0 && id < listData_.size()) ? listData_[id] : nullptr; }

	bool AddShotDataList(const std::wstring& path, bool bReload);
};

class StgShotData {
	friend StgShotDataList;
public:
	struct AnimationData {
		DxRect<LONG> rcSrc_;
		DxRect<float> rcDst_;
		size_t frame_;

		DxRect<LONG>* GetSource() { return &rcSrc_; }
		DxRect<float>* GetDest() { return &rcDst_; }

		static DxRect<float> SetDestRect(DxRect<LONG>* src);
	};
private:
	StgShotDataList* listShotData_;

	int indexTexture_;
	D3DXVECTOR2 textureSize_;

	BlendMode typeRender_;
	BlendMode typeDelayRender_;

	int alpha_;

	AnimationData delay_;
	D3DCOLOR colorDelay_;

	std::vector<AnimationData> listAnime_;
	size_t totalAnimeFrame_;

	std::vector<DxCircle> listCol_;

	double angularVelocityMin_;
	double angularVelocityMax_;
	bool bFixedAngle_;
public:
	StgShotData(StgShotDataList* listShotData);
	virtual ~StgShotData();

	int GetTextureIndex() { return indexTexture_; }
	D3DXVECTOR2& GetTextureSize() { return textureSize_; }

	BlendMode GetRenderType() { return typeRender_; }
	BlendMode GetDelayRenderType() { return typeDelayRender_; }

	int GetAlpha() { return alpha_; }

	DxRect<LONG>* GetDelayRect() { return &delay_.rcSrc_; }
	DxRect<float>* GetDelayDest() { return &delay_.rcDst_; }
	D3DCOLOR GetDelayColor() { return colorDelay_; }

	AnimationData* GetData(size_t frame);
	size_t GetFrameCount() { return listAnime_.size(); }

	std::vector<DxCircle>& GetIntersectionCircleList() { return listCol_; }

	double GetAngularVelocityMin() { return angularVelocityMin_; }
	double GetAngularVelocityMax() { return angularVelocityMax_; }
	bool IsFixedAngle() { return bFixedAngle_; }

	shared_ptr<Texture> GetTexture() { return listShotData_->GetTexture(indexTexture_); }
	StgShotRenderer* GetRenderer() { return GetRenderer(typeRender_); }
	StgShotRenderer* GetRenderer(BlendMode type);
};

//*******************************************************************
//StgShotRenderer
//*******************************************************************
class StgShotRenderer : public RenderObjectTLX {
	friend class StgShotManager;

	std::vector<uint32_t> vecIndex_;

	size_t countMaxVertex_;
	size_t countRenderVertex_;
	size_t countMaxIndex_;
	size_t countRenderIndex_;
public:
	StgShotRenderer();
	~StgShotRenderer();

	virtual void Render(StgShotManager* manager);
	void AddSquareVertex(VERTEX_TLX* listVertex);
	void AddSquareVertex_CurveLaser(VERTEX_TLX* listVertex, bool bAddIndex);

	virtual size_t GetVertexCount() {
		return std::min(countRenderVertex_, vertex_.size() / strideVertexStreamZero_);
	}
	virtual void SetVertexCount(size_t count) {
		vertex_.resize(count * strideVertexStreamZero_);
	}
private:
	inline void AddVertex(VERTEX_TLX& vertex) {
		VERTEX_TLX* data = (VERTEX_TLX*)&vertex_[0];
		memcpy((VERTEX_TLX*)(data + countRenderVertex_), &vertex, strideVertexStreamZero_);
		++countRenderVertex_;
	}
	void TryExpandVertex(size_t chk) {
		//Expands the vertex buffer if its size is insufficient for the next batch
		if (chk < countMaxVertex_ - 6U) return;
		countMaxVertex_ *= 2U;
		SetVertexCount(countMaxVertex_);
	}
	void TryExpandIndex(size_t chk) {
		//Expands the index buffer if its size is insufficient for the next batch
		if (chk < countMaxIndex_ - 6U) return;
		countMaxIndex_ *= 2U;
		vecIndex_.resize(countMaxIndex_);
	}
};

//*******************************************************************
//StgShotObject
//*******************************************************************
struct StgPatternShotTransform;
class StgShotObject : public DxScriptRenderObject, public StgMoveObject, public StgIntersectionObject {
	friend StgPatternShotTransform;
public:
	enum {
		OWNER_PLAYER = 0,
		OWNER_ENEMY,
		OWNER_NULL,

		FRAME_FADEDELETE = 30,
		FRAME_FADEDELETE_LASER = 30,
	};
public:
	struct DelayParameter {
		using lerp_func = Math::Lerp::funcLerp<float, float>;
		enum {
			DELAY_DEFAULT,
			DELAY_LERP,
		};

		int time;
		int id;
		BlendMode blend;
		D3DXVECTOR3 scale;	//[end, start, factor]
		D3DXVECTOR3 alpha;	//[end, start, factor]
		D3DCOLOR colorRep;
		bool colorMix;
		D3DXVECTOR2 angle;	//[angle, spin]

		uint8_t type;		//0 = default danmakufu, 1 = ZUN-like
		lerp_func scaleLerpFunc;	//Scale interpolation
		lerp_func alphaLerpFunc;	//Alpha interpolation

		DelayParameter() : time(0), id(-1), blend(MODE_BLEND_NONE), type(0), colorMix(false) {
			scale = D3DXVECTOR3(0.5f, 2.0f, 15.0f);
			alpha = D3DXVECTOR3(1.0f, 1.0f, 15.0f);
			scaleLerpFunc = Math::Lerp::Linear<float, float>;
			alphaLerpFunc = Math::Lerp::Linear<float, float>;
			colorRep = 0x00000000;
			angle = D3DXVECTOR2(0, 0);
		}
		DelayParameter(float sMin, float sMax, float rate) : time(0), id(-1), blend(MODE_BLEND_NONE), type(0), colorMix(false) {
			scale = D3DXVECTOR3(sMin, sMax, rate);
			alpha = D3DXVECTOR3(1.0f, 1.0f, 15.0f);
			scaleLerpFunc = Math::Lerp::Linear<float, float>;
			alphaLerpFunc = Math::Lerp::Linear<float, float>;
			colorRep = 0x00000000;
			angle = D3DXVECTOR2(0, 0);
		}
		DelayParameter& operator=(const DelayParameter& source) = default;

		inline float GetScale() { return _CalculateValue(&scale, scaleLerpFunc); }
		inline float GetAlpha() { return _CalculateValue(&alpha, alphaLerpFunc); }
		float _CalculateValue(D3DXVECTOR3* param, lerp_func func);

	};
protected:
	StgStageController* stageController_;

	ref_unsync_weak_ptr<StgShotObject> pOwnReference_;

	int frameWork_;
	int idShotData_;
	int typeOwner_;

	D3DXVECTOR2 move_;
	double lastAngle_;

	D3DXVECTOR2 hitboxScale_;

	DelayParameter delay_;

	int frameGrazeInvalid_;
	int frameGrazeInvalidStart_;
	int frameFadeDelete_;

	bool bPenetrateShot_; // Translation: Does The Shot Lose Penetration Points Upon Colliding With Another Shot And Not An Enemy

private:
	struct _WeakPtrHasher {
		std::size_t operator()(const ref_unsync_weak_ptr<StgEnemyObject>& k) const {
			return std::hash<StgEnemyObject*>{}(k.get());
		}
	};
public:
	uint32_t frameEnemyHitInvalid_;
	std::unordered_map<ref_unsync_weak_ptr<StgEnemyObject>, uint32_t, _WeakPtrHasher> mapEnemyHitCooldown_;
	
	bool bRequestedPlayerDeleteEvent_;
	double damage_;
	double life_;

	bool bAutoDelete_;
	bool bEraseShot_;
	bool bSpellFactor_;
	bool bSpellResist_;
	int frameAutoDelete_;
	
	IntersectionListType listIntersectionTarget_;
	bool bUserIntersectionMode_;
	bool bIntersectionEnable_;
	bool bChangeItemEnable_;

	bool bEnableMotionDelay_;
	bool bRoundingPosition_;
	double roundingAngle_;

	StgShotData* _GetShotData() { return _GetShotData(idShotData_); }
	StgShotData* _GetShotData(int id);

	void _SetVertexPosition(VERTEX_TLX* vertex, float x, float y, float z = 1.0f, float w = 1.0f);
	void _SetVertexUV(VERTEX_TLX* vertex, float u, float v);
	void _SetVertexColorARGB(VERTEX_TLX* vertex, D3DCOLOR color);

	virtual void _DeleteInLife();
	virtual void _DeleteInAutoClip();
	virtual void _DeleteInFadeDelete();
	void _DeleteInAutoDeleteFrame();
	void _CommonWorkTask();

	virtual void _Move();

	virtual void _SendDeleteEvent(int type) {}
	void _RequestPlayerDeleteEvent(int hitObjectID);

	std::list<StgPatternShotTransform> listTransformationShotAct_;
	int timerTransform_;
	int timerTransformNext_;
	void _ProcessTransformAct();
public:
	StgShotObject(StgStageController* stageController);
	virtual ~StgShotObject();

	virtual bool HasNormalRendering() { return false; }

	virtual void Work();
	virtual void Render() {}
	virtual void Activate() {}
	virtual void RenderOnShotManager() {}

	virtual void DeleteImmediate();
	virtual void ConvertToItem();

	virtual void Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget);
	virtual void ClearShotObject() { ClearIntersectionRelativeTarget(); }
	virtual void RegistIntersectionTarget() = 0;

	virtual void SetX(float x) { posX_ = x; DxScriptRenderObject::SetX(x); }
	virtual void SetY(float y) { posY_ = y; DxScriptRenderObject::SetY(y); }
	virtual void SetColor(int r, int g, int b);
	virtual void SetAlpha(int alpha);
	virtual void SetRenderState() {}

	void SetTransformList(const std::list<StgPatternShotTransform>& listTransform) {
		listTransformationShotAct_ = listTransform;
	}

	void SetOwnObjectReference();

	int GetShotDataID() { return idShotData_; }
	virtual void SetShotDataID(int id) { idShotData_ = id; }
	int GetOwnerType() { return typeOwner_; }
	void SetOwnerType(int type) { typeOwner_ = type; }

	void SetGrazeInvalidFrame(int frame) { frameGrazeInvalidStart_ = frame; }
	int GetGrazeInvalidFrame() { return frameGrazeInvalidStart_; }
	void SetGrazeFrame(int frame) { frameGrazeInvalid_ = frame; }
	bool IsValidGraze() { return frameGrazeInvalid_ <= 0; }

	void SetPenetrateShotEnable(bool enable) { bPenetrateShot_ = enable; }
	bool GetPenetrateShotEnable() { return bPenetrateShot_; }

	void SetEnemyIntersectionInvalidFrame(uint32_t frame) { frameEnemyHitInvalid_ = frame; }
	uint32_t GetEnemyIntersectionInvalidFrame() { return frameEnemyHitInvalid_;  }

	//Returns true if obj is on hit cooldown
	bool CheckEnemyHitCooldownExists(ref_unsync_weak_ptr<StgEnemyObject> obj);
	void AddEnemyHitCooldown(ref_unsync_weak_ptr<StgEnemyObject> obj, uint32_t time);

	int GetDelay() { return delay_.time; }
	void SetDelay(int delay) { delay_.time = delay; }
	int GetShotDataDelayID() { return delay_.id; }
	void SetShotDataDelayID(int id) { delay_.id = id; }
	BlendMode GetDelayBlendType() { return delay_.blend; }
	void SetDelayBlendType(BlendMode type) { delay_.blend = type; }
	DelayParameter* GetDelayParameter() { return &delay_; }
	void SetDelayParameter(DelayParameter& param) { delay_ = param; }
	void SetEnableDelayMotion(bool b) { bEnableMotionDelay_ = b; }
	void SetDelayAngularVelocity(float av) { delay_.angle.y = av; }

	double GetLife() { return life_; }
	void SetLife(double life) { life_ = life; }
	double GetDamage() { return damage_; }
	void SetDamage(double damage) { damage_ = damage; }
	virtual void SetFadeDelete() { if (frameFadeDelete_ < 0) frameFadeDelete_ = FRAME_FADEDELETE; }
	bool IsAutoDelete() { return bAutoDelete_; }
	void SetAutoDelete(bool b) { bAutoDelete_ = b; }
	void SetAutoDeleteFrame(int frame) { frameAutoDelete_ = frame; }
	bool IsEraseShot() { return bEraseShot_; }
	void SetEraseShot(bool bErase) { bEraseShot_ = bErase; }
	bool IsSpellFactor() { return bSpellFactor_; }
	void SetSpellFactor(bool bSpell) { bSpellFactor_ = bSpell; }
	bool IsSpellResist() { return bSpellResist_; }
	void SetSpellResist(bool bSpell) { bSpellResist_ = bSpell; }

	void SetUserIntersectionMode(bool b) { bUserIntersectionMode_ = b; }
	void SetIntersectionEnable(bool b) { bIntersectionEnable_ = b; }
	bool IsIntersectionEnable() { return bIntersectionEnable_; }
	void SetItemChangeEnable(bool b) { bChangeItemEnable_ = b; }

	void SetPositionRounding(bool b) { bRoundingPosition_ = b; }
	void SetAngleRounding(double a) { roundingAngle_ = a; }

	void SetHitboxScale(D3DXVECTOR2& sc) { hitboxScale_ = sc; }
	void SetHitboxScaleX(float x) { hitboxScale_.x = x; }
	void SetHitboxScaleY(float y) { hitboxScale_.y = y; }
	float GetHitboxScaleX() { return hitboxScale_.x; }
	float GetHitboxScaleY() { return hitboxScale_.y; }
};

//*******************************************************************
//StgNormalShotObject
//*******************************************************************
class StgNormalShotObject : public StgShotObject {
	friend class StgShotObject;
protected:
	double angularVelocity_;
	bool bFixedAngle_;

	void _AddIntersectionRelativeTarget();
	virtual void _SendDeleteEvent(int type);
public:
	StgNormalShotObject(StgStageController* stageController);
	virtual ~StgNormalShotObject();

	virtual void Work();
	virtual void RenderOnShotManager();

	virtual void ClearShotObject() {
		ClearIntersectionRelativeTarget();
	}
	virtual void RegistIntersectionTarget() {
		if (!bUserIntersectionMode_)
			_AddIntersectionRelativeTarget();
	}
	virtual IntersectionListType GetIntersectionTargetList();
	virtual bool GetIntersectionTargetList_NoVector(StgShotData* shotData);

	virtual void SetShotDataID(int id);
	void SetGraphicAngularVelocity(double agv) { angularVelocity_ = agv; }
	void SetFixedAngle(bool fix) { bFixedAngle_ = fix; }
};

//*******************************************************************
//StgLaserObject(レーザー基本部)
//*******************************************************************
class StgLaserObject : public StgShotObject {
protected:
	int length_;
	float lengthF_;
	int widthRender_;
	int widthIntersection_;
	float extendRate_;
	int maxLength_;
	float invalidLengthStart_;
	float invalidLengthEnd_;
	float itemDistance_;

	void _AddIntersectionRelativeTarget();
	void _ExtendLength();
public:
	StgLaserObject(StgStageController* stageController);

	virtual void ClearShotObject() {
		ClearIntersectionRelativeTarget();
	}
	virtual void RegistIntersectionTarget() {
		if (!bUserIntersectionMode_)
			_AddIntersectionRelativeTarget();
	}

	virtual IntersectionListType GetIntersectionTargetList();
	virtual bool GetIntersectionTargetList_NoVector(StgShotData* shotData) { return false; }

	int GetLength() { return length_; }
	void SetLength(int length) { length_ = length; lengthF_ = (float)length; }
	int GetRenderWidth() { return widthRender_; }
	void SetRenderWidth(int width) {
		width = std::max(width, 0);
		widthRender_ = width;
		if (widthIntersection_ < 0) widthIntersection_ = width / 4;
	}
	void SetExtendRate(float rate) { extendRate_ = rate; }
	void SetMaxLength(int max) { maxLength_ = max; }
	int GetIntersectionWidth() { return widthIntersection_; }
	void SetIntersectionWidth(int width) { widthIntersection_ = std::max(width, 0); }
	void SetInvalidLength(float start, float end) { invalidLengthStart_ = start; invalidLengthEnd_ = end; }

	void SetItemDistance(float dist) { itemDistance_ = std::max(dist, 0.1f); }
	float GetItemDistance() { return itemDistance_; }
};

//*******************************************************************
//StgLooseLaserObject(射出型レーザー)
//*******************************************************************
class StgLooseLaserObject : public StgLaserObject {
protected:
	double posXE_;
	double posYE_;

	D3DXVECTOR2 posOrigin_;

	virtual void _DeleteInAutoClip();
	virtual void _Move();
	virtual void _SendDeleteEvent(int type);
public:
	StgLooseLaserObject(StgStageController* stageController);

	virtual void Work();
	virtual void RenderOnShotManager();

	virtual bool GetIntersectionTargetList_NoVector(StgShotData* shotData);

	virtual void SetX(float x) { StgShotObject::SetX(x); posXE_ = x; }
	virtual void SetY(float y) { StgShotObject::SetY(y); posYE_ = y; }
};

//*******************************************************************
//StgStraightLaserObject(設置型レーザー)
//*******************************************************************
class StgStraightLaserObject : public StgLaserObject {
protected:
	double angLaser_;
	double angVelLaser_;

	bool bUseSouce_;
	bool bUseEnd_;
	int idImageEnd_;

	D3DXVECTOR2 delaySize_;

	float scaleX_;

	bool bLaserExpand_;

	virtual void _DeleteInAutoClip();
	virtual void _SendDeleteEvent(int type);
public:
	StgStraightLaserObject(StgStageController* stageController);

	virtual void Work();
	virtual void RenderOnShotManager();

	virtual bool GetIntersectionTargetList_NoVector(StgShotData* shotData);

	double GetLaserAngle() { return angLaser_; }
	void SetLaserAngle(double angle) { angLaser_ = angle; }
	void SetLaserAngularVelocity(double angVel) { angVelLaser_ = angVel; }
	void SetFadeDelete() { if (frameFadeDelete_ < 0) frameFadeDelete_ = FRAME_FADEDELETE_LASER; }

	void SetSourceEnable(bool bEnable) { bUseSouce_ = bEnable; }
	void SetEndEnable(bool bEnable) { bUseEnd_ = bEnable; }
	void SetEndGraphic(int gr) { idImageEnd_ = gr; }
	void SetEndPosition(float x, float y) {
		SetLength(hypotf(x - position_.x, y - position_.y));
		extendRate_ = 0;
		maxLength_ = 0;
		angLaser_ = atan2f(y - position_.y, x - position_.x);
	}
	
	D3DXVECTOR2 GetEndPosition() {
		return D3DXVECTOR2(position_.x + length_ * cosf(angLaser_), position_.y + length_ * sinf(angLaser_));
	}

	void SetSourceEndScale(const D3DXVECTOR2& s) { delaySize_ = s; }

	void SetLaserExpand(bool b) { bLaserExpand_ = b; }
	bool GetLaserExpand() { return bLaserExpand_; }
	
};

//*******************************************************************
//StgCurveLaserObject(曲がる型レーザー)
//*******************************************************************
class StgCurveLaserObject : public StgLaserObject {
public:
	struct LaserNode {
		StgCurveLaserObject* parent;
		D3DXVECTOR2 pos;
		D3DXVECTOR2 vertOff[2];
		D3DCOLOR color;
		float widthMul = 1.0f;
	};
	enum {
		MAP_NORMAL,
		MAP_CAPPED
	};
protected:
	std::list<LaserNode> listPosition_;
	std::list<LaserNode> listPositionC_;

	float posXO_;
	float posYO_;

	float tipDecrement_;

	D3DXVECTOR2 posOrigin_;
	bool bCap_;
	bool bConnect_;
	bool bUniformMove_;
	int smooth_;

	double angLaser_;
	double angVelLaser_;

	virtual void _DeleteInAutoClip();
	virtual void _Move();
	virtual void _SendDeleteEvent(int type);
	void _UpdateConnectedPositionList();
public:
	StgCurveLaserObject(StgStageController* stageController);

	virtual void Work();
	virtual void RenderOnShotManager();

	virtual bool GetIntersectionTargetList_NoVector(StgShotData* shotData);

	void SetTipDecrement(float dec) { tipDecrement_ = dec; }
	void SetTipCapping(bool enable) { bCap_ = enable; }
	void SetTipConnecting(bool enable) { bConnect_ = enable; }
	void SetUniformMotionEnable(bool enable) { bUniformMove_ = enable; }
	void SetAngleSmoothness(int amount) { smooth_ = amount; }

	LaserNode CreateNode(const D3DXVECTOR2& pos, const D3DXVECTOR2& rFac, float widthMul, D3DCOLOR col = 0xffffffff);
	bool GetNode(size_t indexNode, std::list<LaserNode>::iterator& res);
	void GetNodePointerList(std::vector<LaserNode*>* listRes);
	std::list<LaserNode>::iterator PushNode(const LaserNode& node);
};


//*******************************************************************
//StgPatternShotObjectGenerator (ECL-style bullets firing)
//*******************************************************************
class StgPatternShotObjectGenerator : public DxScriptObjectBase {
public:
	enum {
		PATTERN_TYPE_FAN = 0,
		PATTERN_TYPE_FAN_AIMED,
		PATTERN_TYPE_RING,
		PATTERN_TYPE_RING_AIMED,
		PATTERN_TYPE_ARROW,
		PATTERN_TYPE_ARROW_AIMED,
		PATTERN_TYPE_POLYGON,
		PATTERN_TYPE_POLYGON_AIMED,
		PATTERN_TYPE_ELLIPSE,
		PATTERN_TYPE_ELLIPSE_AIMED,
		PATTERN_TYPE_SCATTER_ANGLE,
		PATTERN_TYPE_SCATTER_SPEED,
		PATTERN_TYPE_SCATTER,
        PATTERN_TYPE_LINE,
        PATTERN_TYPE_LINE_AIMED,
		PATTERN_TYPE_ROSE,
		PATTERN_TYPE_ROSE_AIMED,

		BASEPOINT_RESET = -256 * 256,
	};
private:
	StgStageController* stageController_;
	ref_unsync_weak_ptr<StgMoveObject> parent_;
	ref_unsync_weak_ptr<StgMoveParent> shotParent_;
	bool bAutoDelete_;

	int idShotData_;
	int typeOwner_;
	TypeObject typeShot_;
	int typePattern_;
	BlendMode iniBlendType_;

	size_t shotWay_;
	size_t shotStack_;

	//Calculate the sets in order-------------------------------------
	//Set 1
	float basePointX_;
	float basePointY_;
	//Set 2
	float basePointOffsetX_;
	float basePointOffsetY_;
	//Set 4
	float fireRadiusOffset_;
	//-----------------------------------------------------------------

	double speedBase_;
	double speedArgument_;
	double angleBase_;
	double angleArgument_;

    float extra_;

	int delay_;
	//bool delayMove_;

	int laserWidth_;
	int laserLength_;

	std::vector<StgPatternShotTransform> listTransformation_;
public:
	StgPatternShotObjectGenerator(StgStageController* stageController);
	~StgPatternShotObjectGenerator();

	virtual void Render() {}
	virtual void SetRenderState() {}
	virtual void CleanUp();

	void CopyFrom(ref_unsync_ptr<StgPatternShotObjectGenerator> other) {
		StgPatternShotObjectGenerator::CopyFrom(other.get());
	}
	void CopyFrom(StgPatternShotObjectGenerator* other);

	void AddTransformation(StgPatternShotTransform& entry) { listTransformation_.push_back(entry); }
	void SetTransformation(size_t off, StgPatternShotTransform& entry);
	void ClearTransformation() { listTransformation_.clear(); }

	void SetParent(ref_unsync_ptr<StgMoveObject> obj) { parent_ = obj; }
	void SetShotParent(ref_unsync_ptr<StgMoveParent> obj) { shotParent_ = obj; }
	void SetAutoDelete(bool enable) { bAutoDelete_ = enable; }

	void FireSet(void* scriptData, StgStageController* controller, std::vector<int>* idVector);

	void SetGraphic(int id) { idShotData_ = id; }
	void SetTypeOwner(int type) { typeOwner_ = type; }
	void SetTypePattern(int type) { typePattern_ = type; }
	void SetTypeShot(TypeObject type) { typeShot_ = type; }
	void SetBlendType(BlendMode type) { iniBlendType_ = type; }

	void SetWayStack(size_t way, size_t stack) {
		shotWay_ = way;
		shotStack_ = stack;
	};

	void SetBasePoint(float bx, float by) {
		basePointX_ = bx;
		basePointY_ = by;
	}
	void SetOffsetFromBasePoint(float ox, float oy) {
		basePointOffsetX_ = ox;
		basePointOffsetY_ = oy;
	}
	void SetRadiusFromFirePoint(float r) { fireRadiusOffset_ = r; }

	void SetSpeed(double base, double arg) {
		speedBase_ = base;
		speedArgument_ = arg;
	}
	void SetAngle(double base, double arg) {
		angleBase_ = base;
		angleArgument_ = arg;
	}
    void SetExtraData(float e) {
        extra_ = e;
    }

	void SetDelay(int delay) { delay_ = delay; }
	//void SetDelayMotion(bool b) { delayMove_ = b; }
	void SetLaserArgument(int width, int length) {
		laserWidth_ = width;
		laserLength_ = length;
	}
};
struct StgPatternShotTransform {
	enum : uint8_t {
		TRANSFORM_WAIT,
		TRANSFORM_ADD_SPEED_ANGLE,
		TRANSFORM_ANGULAR_MOVE,
		TRANSFORM_N_DECEL_CHANGE,
		TRANSFORM_GRAPHIC_CHANGE,
		TRANSFORM_BLEND_CHANGE,
		TRANSFORM_TO_SPEED_ANGLE,
		TRANSFORM_ADDPATTERN_A1,
		TRANSFORM_ADDPATTERN_A2,
		TRANSFORM_ADDPATTERN_B1,
		TRANSFORM_ADDPATTERN_B2,
		TRANSFORM_ADDPATTERN_C1,
		TRANSFORM_ADDPATTERN_C2,
		//TRANSFORM_,
	};
	uint8_t act = 0xff;
	double param[8];
};