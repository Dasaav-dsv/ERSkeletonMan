#pragma once

namespace HkModifier {
	// Sets a bone to an absolute length.
	class SetLength : public Modifier {
	public:
		SetLength(float length) : length(length) {}
		virtual SetLength* clone() { return new SetLength(*this); }

		virtual bool onApply(Bone* bone, BoneData& bData) { if (std::isfinite(length)) bData.xzyVec = bData.xzyVec.scaleTo(this->length); return false; }

		float length;
	};

	// Scales the bone's length.
	class ScaleLength : public Modifier {
	public:
		ScaleLength(float scale) : scale(std::isfinite(scale) ? scale : 1.0f) {}
		virtual ScaleLength* clone() { return new ScaleLength(*this); }

	private:
		virtual bool onApply(Bone* bone, BoneData& bData) { bData.xzyVec *= this->scale; return false; }

		float scale;
	};

	// Sets the bone's absolute size, not to be confused with its length.
	class SetSize : public Modifier {
	public:
		SetSize(V4D scale) : scale(scale) {}
		virtual SetSize* clone() { return new SetSize(*this); }

		virtual bool onApply(Bone* bone, BoneData& bData) { if (scale.isfinite()) bData.xzyScale = this->scale; return true; }

		V4D scale;
	};

	// Scales the bone's size, not to be confused with its length.
	class ScaleSize : public Modifier {
	public:
		ScaleSize(V4D scale) : scale(scale.isfinite() ? scale : V4D(1.0f)) {}
		virtual ScaleSize* clone() { return new ScaleSize(*this); }

		virtual bool onApply(Bone* bone, BoneData& bData) { bData.xzyScale = _mm_mul_ps(bData.xzyScale, this->scale); return true; }

		V4D scale;
	};

	// Offsets the bone in space by a 3D vector.
	class Offset : public Modifier {
	public:
		Offset(V4D offset) : offset(offset.isfinite() ? offset.flatten<V4D::CoordinateAxis::W>() : V4D(0.0f)) {}
		virtual Offset* clone() { return new Offset(*this); }

		virtual bool onApply(Bone* bone, BoneData& bData) { bData.xzyVec += offset.qTransform(bone->getWorldQ()); return false; }

		V4D offset;
	};

	// Rotates the bone by a quaternion.
	// Tip: use this site https://www.andre-gaschler.com/rotationconverter/
	class Rotate : public Modifier {
	public:
		Rotate(V4D q) : q(q.isfinite() && !q.iszero() ? q.normalize() : V4D(0.0f, 0.0f, 0.0f, 1.0f)) {}
		virtual Rotate* clone() { return new Rotate(*this); }

		virtual bool onApply(Bone* bone, BoneData& bData) { bData.qSpatial = bData.qSpatial.qMul(q).normalize(); return false; }

		V4D q;
	};

	// Disables cloth physics for a character.
	// Must be applied as a skeleton modifier.
	class DisableClothPhysics : public Modifier {
	public:
		DisableClothPhysics() {}
		virtual DisableClothPhysics* clone() { return new DisableClothPhysics(*this); }

		virtual bool onApply(Bone* bone, BoneData& bData) { *PointerChain::make<int>(bone->getSkeleton()->getChrIns(), 0x548) = 1; return true; } //, 0x190, 0xE8, 0x163
	};

	namespace Mounted {
		// Disables cloth physics for a character when they are mounted.
		// Must be applied as a skeleton modifier.
		class DisableClothPhysics : public Modifier {
		public:
			DisableClothPhysics() {}
			virtual DisableClothPhysics* clone() { return new DisableClothPhysics(*this); }

			virtual bool onApply(Bone* bone, BoneData& bData) 
			{ 
				auto clothState = PointerChain::make<int>(bone->getSkeleton()->getChrIns(), 0x548);
				if (PointerChain::make<bool>(bone->getSkeleton()->getChrIns(), 0x190, 0xE8u, 0x163u).dereference(false)) {
					*clothState = 1;
				}
				else {
					*clothState = *clothState == 1 ? -5 : *clothState;
				}
				return true;
			}
		};
	}
}