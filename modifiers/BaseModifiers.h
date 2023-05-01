#pragma once

namespace HkModifier {
	// Sets a bone to an absolute length.
	class SetLength : public Modifier {
	public:
		SetLength(float length) : length(length) {}
		virtual SetLength* clone() { return new SetLength(*this); }

		virtual void onApply(Bone* bone, BoneData& bData) { if (std::isfinite(length)) bData.xzyVec = bData.xzyVec.scaleTo(this->length); }

		float length;
	};

	// Scales the bone's length.
	class ScaleLength : public Modifier {
	public:
		ScaleLength(float scale) : scale(std::isfinite(scale) ? scale : 1.0f) {}
		virtual ScaleLength* clone() { return new ScaleLength(*this); }

	private:
		virtual void onApply(Bone* bone, BoneData& bData) { bData.xzyVec *= this->scale; }

		float scale;
	};

	// Sets the bone's absolute size, not to be confused with its length.
	class SetSize : public Modifier {
	public:
		SetSize(V4D scale) : scale(scale) {}
		virtual SetSize* clone() { return new SetSize(*this); }

		virtual void onApply(Bone* bone, BoneData& bData) { if (scale.isfinite()) bData.xzyScale = this->scale; }

		V4D scale;
	};

	// Scales the bone's size, not to be confused with its length.
	class ScaleSize : public Modifier {
	public:
		ScaleSize(V4D scale) : scale(scale.isfinite() ? scale : V4D(1.0f)) {}
		virtual ScaleSize* clone() { return new ScaleSize(*this); }

		virtual void onApply(Bone* bone, BoneData& bData) { bData.xzyScale = _mm_mul_ps(bData.xzyScale, this->scale); }

		V4D scale;
	};

	// Offsets the bone in space by a 3D vector.
	class Offset : public Modifier {
	public:
		Offset(V4D offset) : offset(offset.isfinite() ? offset.flatten<V4D::CoordinateAxis::W>() : V4D(0.0f)) {}
		virtual Offset* clone() { return new Offset(*this); }

		virtual void onApply(Bone* bone, BoneData& bData) { bData.xzyVec += offset.qTransform(bone->getWorldQ()); }

		V4D offset;
	};

	// Rotates the bone by a quaternion.
	// Tip: use this site https://www.andre-gaschler.com/rotationconverter/
	class Rotate : public Modifier {
	public:
		Rotate(V4D q) : q(q.isfinite() && !q.iszero() ? q.normalize() : V4D(0.0f, 0.0f, 0.0f, 1.0f)) {}
		virtual Rotate* clone() { return new Rotate(*this); }

		virtual void onApply(Bone* bone, BoneData& bData) { bData.qSpatial = bData.qSpatial.qMul(q).normalize(); }

		V4D q;
	};
}