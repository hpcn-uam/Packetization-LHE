--******************************************************************************
-- To use in Wireshark:
-- 1) Ensure your Wireshark works with Lua plugins
--    1.a) "About Wireshark" should say it is compiled with Lua
-- 2) Install this dissector in the proper plugin directory
--    2.a) "About Wireshark/Folders" to see Personal and Global plugin 
--         directories.
--    2.b) After putting this dissector in the proper folder,
--         "About Wireshark/Plugins" should list "RTP_LHE.lua" 
-- 3) In Wireshark Preferences, under "Protocols", set RTP_LHE as 
--    dynamic payload type 124
-- 4) Capture packets of LHE
-- 5) "Decode As" those UDP packets as RTP
-- 6) You will now see the LHE Data dissection of the RTP payload
--******************************************************************************
do

	--[[
		LHE profile dissector, as specific RTP payload type
		We can dynamically change the specific value in preferences
	--]]
	local rtp_lhe = Proto ("rtp_lhe","LHE over RTP")
	local prefs = rtp_lhe.prefs
	prefs.dyn_pt = Pref.uint("LHE over RTP",
	                         124,
	                         "A dynamic value (Default: 124)")

	local F = rtp_lhe.fields

	--**************************************************************************
	-- Global LHE profile header
	--**************************************************************************
	F.version = ProtoField.uint8("rtp_lhe.version", "LHE version", base.BIN)

	A_values={[0]="Video",
	          [1]="Audio"}
	F.A = ProtoField.uint8("rtp_lhe.A", "LHE Media Type", base.DEC, A_values)

	D_values={[0]="Not differential",
	          [1]="Differential"}
	F.D = ProtoField.uint8("rtp_lhe.D", "Differential encoding",
	                       base.HEX, D_values)

	-- Following LHE over RTP specification Rev 1, if the value of this field
	-- is not included in this list, it should be considered Reserved
	profile_values={[0]="YUV 4:0:0",
	                [1]="YUV 4:2:0",
	                [2]="YUV 4:2:2",
	                [3]="YUV 4:4:4",}

	F.chroma = ProtoField.uint8("rtp_lhe.chroma", "Chroma subsampling",
	                            base.DEC, profile_values)

	-- Following LHE over RTP specification Rev 1, if the value of this field
	-- is not included in this list, it should be considered Reserved
	codec_values={[0]="LHE (Basic)", 
	              [1]="LHE (Advanced)",
	              [2]="LHE2",
	              [3]="Reserved"}

	F.codec = ProtoField.uint8("rtp_lhe.codec", "Codec description",
	                           base.DEC, codec_values)

	F.R = ProtoField.uint8("rtp_lhe.R", "Reserved bits", base.DEC)

	F.nblocks = ProtoField.uint8("rtp_lhe.nblocks", "Number of blocks", base.DEC)

	F.bh = ProtoField.uint16("rtp_lhe.bh", "Block Height (px)", base.DEC)
	F.bw = ProtoField.uint16("rtp_lhe.bw", "Block Width (px)", base.DEC)

	F.ih = ProtoField.uint32("rtp_lhe.ih", "Image Height (px)", base.DEC)
	F.iw = ProtoField.uint32("rtp_lhe.iw", "Image Width (px)", base.DEC)

	F.fbi = ProtoField.uint16("rtp_lhe.fbi", "First block ID", base.HEX)

	--**************************************************************************
	-- Block header
	--**************************************************************************
	btype_values = {[0] = "Not Differential", [1] = "Differential"}

	F.b_type = ProtoField.uint8("rtp_lhe.b_type","Block type",
	                            base.BIN,btype_values)

	F.b_len = ProtoField.uint16("rtp_lhe.b_len","Block lenght",base.DEC)
	F.b_payload = ProtoField.bytes("rtp_lhe.b_payload","Block payload")

	function rtp_lhe.dissector (buffer, pinfo, tree)

		local n_blocks = 0
		local subtree = tree:add(rtp_lhe, buffer(),"LHE Data")

		local block_h = 0
		local block_w = 0
		local im_h = 0
		local im_w = 0

		local version_aux = buffer(0,1):bitfield(0,2)
		subtree:add(F.version, buffer(0,1), version_aux)

		if (version_aux == 0) then

			subtree:add(F.A, buffer(0,1), buffer(0,1):bitfield(2,1))
			subtree:add(F.D, buffer(0,1), buffer(0,1):bitfield(3,1))
			subtree:add(F.chroma, buffer(0,1), buffer(0,1):bitfield(4,4))
			subtree:add(F.R, buffer(1,1), buffer(1,1):bitfield(0,2))

			n_blocks = buffer(1,1):bitfield(2,6)
			subtree:add(F.nblocks, buffer(1,1), n_blocks)

			block_h = buffer(2,1):uint()*2
			block_w = buffer(3,1):uint()*2
			subtree:add(F.bh, buffer(2,1),block_h)
			subtree:add(F.bw, buffer(3,1),block_w)

			im_h = buffer(4,2):uint()*block_h
			im_w = buffer(6,2):uint()*block_w
			subtree:add(F.ih, buffer(4,2),im_h)
			subtree:add(F.iw, buffer(6,2),im_w)

		elseif (version_aux == 1) then

			subtree:add(F.A, buffer(0,1), buffer(0,1):bitfield(2,1))
			subtree:add(F.R, buffer(1,1), buffer(1,1):bitfield(0,1))
			local chroma_aux = buffer(0,1):bitfield(4,4)
			subtree:add(F.chroma, buffer(0,1), 
			            chroma_aux,
			            string.format("Chroma subsampling: %s (%x)",
			                          profile_values[chroma_aux] or "Reserved",
			                          chroma_aux))

			subtree:add(F.codec, buffer(1,1), buffer(1,1):bitfield(0,2))
			n_blocks = buffer(1,1):bitfield(2,6)
			subtree:add(F.nblocks, buffer(1,1), n_blocks)

			block_h = buffer(2,1):bitfield(0,8)+1
			block_w = (buffer(2,2):bitfield(8,8)+1)*16
			subtree:add(F.bh, buffer(2,1),block_h)
			subtree:add(F.bw, buffer(2,2),block_w)

			local im_h = buffer(4,2):uint()*block_h
			local im_w = buffer(6,2):uint()*block_w
			subtree:add(F.ih, buffer(4,2),im_h)
			subtree:add(F.iw, buffer(6,2),im_w)
		end

		subtree:add(F.fbi, buffer(8,2))

		local n_blocks_index = 0
		local offset = 10
		local b_len_aux = 0
		while n_blocks_index < n_blocks do

			b_len_aux = buffer(offset,2):bitfield(3,13)
			local block_tree = subtree:add(rtp_lhe,
			                               buffer(offset,b_len_aux),
			                               "[LHE block data]")
			local b_type_aux = buffer(offset,1):bitfield(0,3)

			block_tree:add(F.b_type, buffer(offset,1),
			               b_type_aux,
			               string.format("Block type: %s (%x)",
			                             btype_values[b_type_aux] or "Reserved",
			                             b_type_aux))

			block_tree:add(F.b_len, buffer(offset,2),b_len_aux)
			offset = offset + 2
			block_tree:add(F.b_payload, buffer(offset,b_len_aux))
			offset = offset + b_len_aux
			n_blocks_index = n_blocks_index + 1
		end

		return true
	end

	--**************************************************************************
	-- Register dissector to dynamic payload type dissectorTable
	--**************************************************************************
	local dyn_payload_type_table = DissectorTable.get("rtp_dyn_payload_type")
	dyn_payload_type_table:add("LHE over RTP", rtp_lhe)

	--**************************************************************************
	-- Register dissector to RTP payload type
	--**************************************************************************
	local payload_type_table = DissectorTable.get("rtp.pt")
	local old_dissector = nil
	local old_dyn_pt = 0

	function rtp_lhe.init()
		if (prefs.dyn_pt ~= old_dyn_pt) then
			if (old_dyn_pt > 0) then
				if (old_dissector == nil) then
					payload_type_table:remove(old_dyn_pt, rtp_lhe)
				else
					payload_type_table:add(old_dyn_pt, old_dissector)
				end
			end
			old_dyn_pt = prefs.dyn_pt
			old_dissector = payload_type_table:get_dissector(old_dyn_pt)
			if (prefs.dyn_pt > 0) then
				payload_type_table:add(prefs.dyn_pt, rtp_lhe)
			end
		end
	end
end


