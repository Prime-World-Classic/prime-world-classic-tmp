/** Compare repeated-index fallback draws against the uncached real OpenGL path. */
bool RunLinuxPreviewVertexReuseEngineProbe()
{
	unsigned checks = 0, failures = 0;
	auto check = [&](bool ok, const char* label) {
		++checks;
		if (!ok) { ++failures; fprintf(stderr, "Vertex reuse FAIL: %s\n", label); }
	};
	struct Vertex { CVec3 position, normal; float uv[2], weights[4]; unsigned char bones[4]; };
	Vertex vertices[6]{};
	for (unsigned i = 0; i < 4; ++i) {
		auto& vertex = vertices[i + 1];
		vertex.position = CVec3(i == 1 || i == 2 ? .7f : -.7f, 0, i < 2 ? -.7f : .7f);
		vertex.normal = CVec3(0, -1, 0);
		vertex.uv[0] = (i == 1 || i == 2) ? 1 : 0; vertex.uv[1] = i >= 2 ? 1 : 0;
		vertex.weights[0] = .6f; vertex.weights[1] = .4f; vertex.bones[1] = 1;
	}
	Render::VertexFormatDescriptor format;
	format.AddVertexElement(Render::VertexElementDescriptor(0, offsetof(Vertex, position), Render::VERTEXELEMENTTYPE_FLOAT3, Render::VERETEXELEMENTUSAGE_POSITION, 0));
	format.AddVertexElement(Render::VertexElementDescriptor(0, offsetof(Vertex, normal), Render::VERTEXELEMENTTYPE_FLOAT3, Render::VERETEXELEMENTUSAGE_NORMAL, 0));
	format.AddVertexElement(Render::VertexElementDescriptor(0, offsetof(Vertex, uv), Render::VERTEXELEMENTTYPE_FLOAT2, Render::VERETEXELEMENTUSAGE_TEXCOORD, 0));
	format.AddVertexElement(Render::VertexElementDescriptor(0, offsetof(Vertex, weights), Render::VERTEXELEMENTTYPE_FLOAT4, Render::VERETEXELEMENTUSAGE_BLENDWEIGHT, 0));
	format.AddVertexElement(Render::VertexElementDescriptor(0, offsetof(Vertex, bones), Render::VERTEXELEMENTTYPE_UBYTE4, Render::VERETEXELEMENTUSAGE_BLENDINDICES, 0));
	std::unique_ptr<IDirect3DVertexBuffer9> vb(Render::CreateVB(sizeof(vertices), Render::RENDER_POOL_MANAGED, vertices).GetPtr());
	const unsigned short indices[] = {4, 4, 0, 1, 2, 0, 2, 3};
	std::unique_ptr<IDirect3DIndexBuffer9> ib(Render::CreateIB16(sizeof(indices), Render::RENDER_POOL_MANAGED, indices).GetPtr());
	const unsigned indices32[] = {0, 0, 3, 4, 5, 3, 5, 6};
	std::unique_ptr<IDirect3DIndexBuffer9> ib32(Render::CreateIB(sizeof(indices32), Render::RENDER_POOL_MANAGED, indices32).GetPtr());
	Render::Primitive primitive;
	primitive.SetVertexDeclaration(Render::SmartRenderer::GetVertexFormatDeclaration(format));
	primitive.SetVertexStream(vb.get(), sizeof(Vertex)); primitive.SetIndexBuffer(ib.get());
	primitive.GetDipDescriptor() = Render::DipDescriptor(Render::RENDERPRIMITIVE_TRIANGLELIST, 1, 0, 4, 2, 2);
	Matrix43 world, bones[2]; Identity(&world); Identity(&bones[0]); Identity(&bones[1]);
	world._14 = .2f; bones[0]._14 = -.3f; bones[1]._34 = .4f;
	glPushAttrib(GL_ALL_ATTRIB_BITS); glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
	GLint mode = 0; glGetIntegerv(GL_MATRIX_MODE, &mode);
	glViewport(0, 0, 128, 128); glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDisable(GL_SCISSOR_TEST); glDisable(GL_LIGHTING); glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
	glColorMask(1, 1, 1, 1); glClearColor(0, 0, 0, 1);
	glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(-2, 2, -2, 2, -20, 20);
	glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
	GLuint texture = 0; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
	const unsigned char colors[] = {255, 40, 80, 255, 30, 255, 70, 255, 70, 60, 255, 255, 255, 255, 255, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	using Pixels = std::array<unsigned char, 128 * 128 * 4>;
	auto draw = [&](bool reuse, bool skin) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		Render::SmartRenderer::SetOpenGLImmediateMeshDrawingEnabled(false);
		Render::SmartRenderer::SetOpenGLImmediateMeshDrawingEnabled(true);
		Render::SmartRenderer::SetOpenGLVertexReuseEnabled(reuse);
		Render::SmartRenderer::ResetOpenGLVertexWorkStats();
		Render::SmartRenderer::SetOpenGLImmediateMeshPreviewTransform(.1f, 0, 0, .8f);
		Render::SmartRenderer::SetOpenGLImmediateObjectMatrix(&world);
		Render::SmartRenderer::SetOpenGLImmediateSkeletalMatrices(skin ? bones : nullptr, skin ? 2 : 0);
		primitive.Bind(); primitive.Draw();
		Pixels pixels{}; glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		Render::SmartRenderer::SetOpenGLImmediateMeshDrawingEnabled(false);
		return pixels;
	};
	for (int range : {0, 1, 2, 3, 4, 5})
	for (bool textured : {false, true})
	for (bool skin : {false, true})
	for (int phase : {0, 1}) {
		if (textured) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
		primitive.SetVertexStream(vb.get(), sizeof(Vertex), range == 2 ? sizeof(Vertex) : 0);
		primitive.SetIndexBuffer(range == 1 ? ib32.get() : ib.get());
		primitive.GetDipDescriptor() = Render::DipDescriptor(Render::RENDERPRIMITIVE_TRIANGLELIST,
			range == 1 ? -2 : (range == 2 ? 0 : 1), range == 1 ? 3 : (range == 4 ? 100 : 0),
			range == 3 ? 0 : (range == 5 ? UINT_MAX : 4), 2, 2);
		world._14 = .2f + phase * .1f; bones[1]._34 = .4f - phase * .2f;
		Vertex changed = vertices[1]; changed.position.x += phase * .1f; changed.uv[0] += phase * .4f;
		memcpy(vb->storage.begin() + sizeof(Vertex), &changed, sizeof(changed));
		const auto reference = draw(false, skin);
		const auto before = Render::SmartRenderer::GetOpenGLVertexWorkStats();
		const auto actual = draw(true, skin);
		const auto after = Render::SmartRenderer::GetOpenGLVertexWorkStats();
		unsigned visible = 0;
		for (size_t i = 0; i < actual.size(); i += 4) if (actual[i] || actual[i + 1] || actual[i + 2]) ++visible;
		check(visible > 500 && actual == reference, "static/skinned pixels match the uncached indexed reference");
		check(before.draws == 1 && before.corners == 6 && before.transformed == 6 && before.reused == 0,
			"uncached oracle transforms every emitted corner");
		const bool cacheable = range < 3;
		check(after.draws == 1 && after.corners == 6 && after.transformed == (cacheable ? 4 : 6) &&
			after.reused == (cacheable ? 2 : 0), "bounded declared ranges reuse vertices; unknown/oversized ranges retain the oracle");
	}
	{
		std::vector<Vertex> wide(258);
		for (unsigned i = 0; i < 4; ++i) wide[i < 2 ? i : i + 254] = vertices[i + 1];
		const unsigned aliasIndices[] = {0, 1, 256, 0, 256, 257};
		std::unique_ptr<IDirect3DVertexBuffer9> wideVb(Render::CreateVB(wide.size() * sizeof(Vertex), Render::RENDER_POOL_MANAGED, wide.data()).GetPtr());
		std::unique_ptr<IDirect3DIndexBuffer9> aliasIb(Render::CreateIB(sizeof(aliasIndices), Render::RENDER_POOL_MANAGED, aliasIndices).GetPtr());
		primitive.SetVertexStream(wideVb.get(), sizeof(Vertex)); primitive.SetIndexBuffer(aliasIb.get());
		primitive.GetDipDescriptor() = Render::DipDescriptor(Render::RENDERPRIMITIVE_TRIANGLELIST, 0, 0, wide.size(), 0, 2);
		const auto reference = draw(false, true), actual = draw(true, true);
		const auto stats = Render::SmartRenderer::GetOpenGLVertexWorkStats();
		check(actual == reference && stats.corners == 6 && stats.transformed == 6 && stats.reused == 0,
			"fixed-cache collisions recompute instead of substituting another vertex");
		primitive.SetVertexStream(vb.get(), sizeof(Vertex)); primitive.SetIndexBuffer(ib.get());
	}
	for (int base : {1, -2}) {
		const unsigned bad = base > 0 ? UINT_MAX : 0;
		for (size_t i = 0; i < sizeof(indices32) / sizeof(unsigned); ++i)
			memcpy(ib32->storage.begin() + i * sizeof(unsigned), &bad, sizeof(bad));
		primitive.SetIndexBuffer(ib32.get()); primitive.SetVertexStream(vb.get(), sizeof(Vertex));
		primitive.GetDipDescriptor() = Render::DipDescriptor(Render::RENDERPRIMITIVE_TRIANGLELIST, base, 0, 4, 2, 2);
		for (bool reuse : {false, true}) {
			draw(reuse, true);
			const auto stats = Render::SmartRenderer::GetOpenGLVertexWorkStats();
			check(stats.draws == 1 && stats.corners == 0 && stats.transformed == 0,
				"overflowing or negative effective indices never alias an in-range cached vertex");
		}
	}
	if (CmdLineLite::Instance().IsKeyDefined("--bootstrap-vertex-reuse-benchmark")) {
		// A 64x64 indexed grid has normal mesh locality rather than repeated identical triangles.
		constexpr unsigned side = 64, repetitions = 20;
		std::vector<Vertex> grid((side + 1) * (side + 1));
		std::vector<unsigned> elements;
		for (unsigned y = 0; y <= side; ++y) for (unsigned x = 0; x <= side; ++x) {
			auto& vertex = grid[y * (side + 1) + x]; vertex = vertices[1];
			vertex.position = CVec3(-.8f + 1.6f * x / side, 0, -.8f + 1.6f * y / side);
		}
		for (unsigned y = 0; y < side; ++y) for (unsigned x = 0; x < side; ++x) {
			const unsigned a = y * (side + 1) + x, b = a + side + 1;
			for (unsigned index : {a, a + 1, b + 1, a, b + 1, b}) elements.push_back(index);
		}
		std::unique_ptr<IDirect3DVertexBuffer9> gridVb(Render::CreateVB(grid.size() * sizeof(Vertex), Render::RENDER_POOL_MANAGED, grid.data()).GetPtr());
		std::unique_ptr<IDirect3DIndexBuffer9> gridIb(Render::CreateIB(elements.size() * sizeof(unsigned), Render::RENDER_POOL_MANAGED, elements.data()).GetPtr());
		primitive.SetVertexStream(gridVb.get(), sizeof(Vertex)); primitive.SetIndexBuffer(gridIb.get());
		primitive.GetDipDescriptor() = Render::DipDescriptor(Render::RENDERPRIMITIVE_TRIANGLELIST, 0, 0, grid.size(), 0, elements.size() / 3);
		glDisable(GL_TEXTURE_2D);
		for (unsigned round = 0; round < 4; ++round) {
			Render::SmartRenderer::SetOpenGLImmediateMeshDrawingEnabled(false);
			Render::SmartRenderer::SetOpenGLImmediateMeshDrawingEnabled(true);
			Render::SmartRenderer::SetOpenGLImmediateSkeletalMatrices(bones, 2);
			Render::SmartRenderer::SetOpenGLVertexReuseEnabled(round % 2);
			primitive.Bind(); primitive.Draw(); glFinish();
			Render::SmartRenderer::ResetOpenGLVertexWorkStats();
			const auto begin = std::chrono::steady_clock::now();
			for (unsigned i = 0; i < repetitions; ++i) { glClear(GL_COLOR_BUFFER_BIT); primitive.Draw(); }
			glFinish();
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
			const auto stats = Render::SmartRenderer::GetOpenGLVertexWorkStats();
			check(stats.transformed == repetitions * (round % 2 ? grid.size() : elements.size()), "benchmark transform counts follow indexed locality");
			fprintf(stdout, "Vertex reuse benchmark: enabled=%u draws=%llu corners=%llu transformed=%llu reused=%llu wallMs=%.3f\n",
				round % 2, stats.draws, stats.corners, stats.transformed, stats.reused, ms);
			Render::SmartRenderer::SetOpenGLImmediateMeshDrawingEnabled(false);
		}
		primitive.SetVertexStream(vb.get(), sizeof(Vertex)); primitive.SetIndexBuffer(ib.get());
	}
	Render::SmartRenderer::SetOpenGLVertexReuseEnabled(true);
	Render::SmartRenderer::UnBindVertexBuffer(0); Render::SmartRenderer::BindIndexBuffer(DXIndexBufferRef());
	glDeleteTextures(1, &texture);
	glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(mode);
	glPopClientAttrib(); glPopAttrib();
	check(glGetError() == GL_NO_ERROR, "vertex reuse leaves no OpenGL error");
	fprintf(stdout, "Vertex reuse: %u checks, %u failures\n", checks, failures);
	return failures == 0;
}
