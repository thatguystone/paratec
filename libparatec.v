LIBPARATEC_1.0 {
	global:
		main;
		pt_*;
		_pt_*;
		extern "C++" {
			std::to_string*;
			*std::equal_to*;
			*pt::assert::*;
		};

	local:
		*;
};

