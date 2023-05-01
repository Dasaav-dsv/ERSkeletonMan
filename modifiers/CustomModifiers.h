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
		virtual void onApply(Bone* bone, BoneData& bData)
		{
			bData.qSpatial = bData.qSpatial.qMul(qAdd).normalize();
			qAdd = qAdd.qMul(q).normalize();

			this->t += *PointerChain::make<float>(bone->getSkeleton()->getChrIns(), 0xB0) * 1.75f;
			bData.xzyVec += V4D(sinf(this->t), 0.0f, cosf(this->t)) * 0.75f;
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

		virtual void onApply(Bone* bone, BoneData& bData)
		{
			bData.qSpatial = bData.qSpatial.qMul(V4D(0.0f, -0.1961161f, 0.0f, 0.9805807f)).qMul(V4D(0.0f, 0.0f, 0.5144958f, 0.8574929f).qPow(sinf(this->t)));
			this->t += *PointerChain::make<float>(bone->getSkeleton()->getChrIns(), 0xB0) * 8.0f;
		}

		float t = 0.0f;
	};
}