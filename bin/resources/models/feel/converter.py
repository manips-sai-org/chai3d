import coacd

mesh = trimesh.load(input_file, force="mesh")
mesh = coacd.Mesh(mesh.vertices, mesh.faces)
parts = coacd.run_coacd(mesh) # a list of convex hulls.