import pcbnew

board = pcbnew.GetBoard()

model_path = r"/PATH/TO/MODEL.step" # Update with your model path

offset = pcbnew.VECTOR3D(4.75, 6.75, -3.5)
rotation = pcbnew.VECTOR3D(180, 180, 0)
scale = pcbnew.VECTOR3D(1, 1, 1)

count = 0
missing = []

for i in range(1, 50):
    ref = f"S{i}"
    fp = board.FindFootprintByReference(ref)
    if not fp:
        missing.append(ref)
        continue

    model = pcbnew.FP_3DMODEL()
    
    # Set path directly on attribute
    model.m_Filename = model_path
    
    # Set transforms directly on attributes
    model.m_Offset = offset
    model.m_Rotation = rotation
    model.m_Scale = scale
    
    # Clear existing models by resetting to empty vector (if supported)
    # But if no SetModels, skip clearing
    
    # Add the model
    fp.Add3DModel(model)

    print(f" Model assigned to {ref}")
    count += 1

pcbnew.Refresh()

print(f"\n Done. Models assigned to {count} switches.")
if missing:
    print(f" Missing footprints: {', '.join(missing)}")
