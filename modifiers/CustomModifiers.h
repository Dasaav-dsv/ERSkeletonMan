#pragma once

namespace HkModifier {
	// An example of a custom modifier.
	// Apply it to the root bone of any character to see what it does.
	// You are free to remove it, replace it or add your own after it.
	class CapriSun : public Modifier {
	public:
		// The constructor. The base constructor can suffice too, 
		// however here I added an additional check that prevents abnormal parameters from being applied.
		CapriSun(V4D q) : q(q.isfinite() && !q.iszero() ? q.normalize() : V4D(0.0f, 0.0f, 0.0f, 1.0f)), qAdd(this->q) {}
		// A polymorphic copy method, it must be defined for every custom modifier.
		virtual CapriSun* clone() { return new CapriSun(*this); }

		// Any custom modifier must have this function defined with this exact signature.
		virtual bool onApply(Bone* bone, BoneData& bData)
		{
			bData.qSpatial = bData.qSpatial.qMul(qAdd).normalize();
			qAdd = qAdd.qMul(q).normalize();

			this->t += *PointerChain::make<float>(bone->getSkeleton()->getChrIns(), 0xB0) * 1.75f;
			bData.xzyVec += V4D(sinf(this->t), 0.0f, cosf(this->t)) * 0.75f;

			return true;
		}

		V4D q;
		V4D qAdd;
		float t = 0.0f;
	};

	// Apply to L_UpperArm and R_UpperArm
	class Floss : public Modifier {
	public:
		Floss() {}
		virtual Floss* clone() { return new Floss(*this); }

		virtual bool onApply(Bone* bone, BoneData& bData)
		{
			bData.qSpatial = bData.qSpatial.qMul(V4D(0.0f, -0.1961161f, 0.0f, 0.9805807f)).qMul(V4D(0.0f, 0.0f, 0.5144958f, 0.8574929f).qPow(sinf(this->t)));
			this->t += *PointerChain::make<float>(bone->getSkeleton()->getChrIns(), 0xB0) * 8.0f;

			return false;
		}

		float t = 0.0f;
	};

	class RotateGlobal : public Modifier {
	public:
		RotateGlobal(V4D q) : q(q) {}
		virtual RotateGlobal* clone() { return new RotateGlobal(*this); }

		virtual void onApply(Bone* bone, BoneData& bData) 
		{
			bData.qSpatial = bone->getWorldQ().qMul(this->q);
		}

		V4D q;
	};

	namespace SpEffect {
		// Scales the bone's length, but only when a chosen SpEffect is applied to the character.
		class ScaleLength : public Modifier {
		public:
			ScaleLength(float scale, int spEffectID) : scale(std::isfinite(scale) ? scale : 1.0f), ID(spEffectID) {}
			virtual ScaleLength* clone() { return new ScaleLength(*this); }

		private:
			virtual bool onApply(Bone* bone, BoneData& bData) { if (Impl::checkSpEffectID(bone->getSkeleton()->getChrIns(), ID)) bData.xzyVec *= this->scale; }

			float scale;
			int ID;
		};

		// Scales the bone's size, but only when a chosen SpEffect is applied to the character.
		class ScaleSize : public Modifier {
		public:
			ScaleSize(V4D scale, int spEffectID) : scale(scale.isfinite() ? scale : V4D(1.0f)), ID(spEffectID) {}
			virtual ScaleSize* clone() { return new ScaleSize(*this); }

			virtual bool onApply(Bone* bone, BoneData& bData) 
			{ 
				if (Impl::checkSpEffectID(bone->getSkeleton()->getChrIns(), ID)) {
					bData.xzyScale = _mm_mul_ps(bData.xzyScale, this->scale);
				}
				return true; 
			}

			V4D scale;
			int ID;
		};

		// Offsets the bone in space by a 3D vector, but only when a chosen SpEffect is applied to the character.
		class Offset : public Modifier {
		public:
			Offset(V4D offset, int spEffectID) : offset(offset.isfinite() ? offset.flatten<V4D::CoordinateAxis::W>() : V4D(0.0f)), ID(spEffectID) {}
			virtual Offset* clone() { return new Offset(*this); }

			virtual bool onApply(Bone* bone, BoneData& bData) 
			{ 
				if (Impl::checkSpEffectID(bone->getSkeleton()->getChrIns(), ID)) {
					bData.xzyVec += offset.qTransform(bone->getWorldQ());
				}
				return false;
			}

			V4D offset;
			int ID;
		};

		// Rotates the bone by a quaternion, but only when a chosen SpEffect is applied to the character.
		// Tip: use this site https://www.andre-gaschler.com/rotationconverter/
		class Rotate : public Modifier {
		public:
			Rotate(V4D q, int spEffectID) : q(q.isfinite() && !q.iszero() ? q.normalize() : V4D(0.0f, 0.0f, 0.0f, 1.0f)), ID(spEffectID) {}
			virtual Rotate* clone() { return new Rotate(*this); }

			virtual bool onApply(Bone* bone, BoneData& bData) 
			{ 
				if (Impl::checkSpEffectID(bone->getSkeleton()->getChrIns(), ID)) {
					bData.qSpatial = bData.qSpatial.qMul(q).normalize();
				}
				return false;
			}

			V4D q;
			int ID;
		};
	}
}
