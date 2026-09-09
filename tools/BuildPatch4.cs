// Build patched Spell.dbc + SkillLineAbility.dbc and pack them into patch-4.MPQ
// Compile: csc /nologo /out:BuildPatch4.exe BuildPatch4.cs
// Usage:   BuildPatch4.exe <dbc-dir> <output-mpq>

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

internal static class Program
{
    const int SpellIdField = 0;
    const int SpellCastTime = 28;
    const int SpellBaseLevel = 38;
    const int SpellSpellLevel = 39;
    const int SpellDieSides1 = 74;
    const int SpellBasePoints1 = 80;
    const int SpellRank = 153;
    const int SpellManaPct = 204;

    const uint GhostWolfId = 2645;
    const uint LavaBurstTemplateId = 51505;

    static readonly Rank[] Ranks =
    {
        new Rank(910001, 10, 81, 95, "Rank 1"),
        new Rank(910002, 20, 150, 171, "Rank 2"),
        new Rank(910003, 30, 309, 350, "Rank 3"),
        new Rank(910004, 40, 408, 460, "Rank 4"),
        new Rank(910005, 50, 624, 700, "Rank 5"),
        new Rank(910006, 60, 891, 1017, "Rank 6"),
        new Rank(910007, 70, 1015, 1160, "Rank 7"),
    };

    static int Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.WriteLine("Usage: BuildPatch4.exe <dbc-dir> <output-mpq>");
            return 1;
        }

        string dbcDir = args[0];
        string mpqPath = args[1];
        string spellPath = Path.Combine(dbcDir, "Spell.dbc");
        string slaPath = Path.Combine(dbcDir, "SkillLineAbility.dbc");
        if (!File.Exists(spellPath) || !File.Exists(slaPath))
        {
            Console.Error.WriteLine("Need Spell.dbc and SkillLineAbility.dbc in " + dbcDir);
            return 1;
        }

        string patchedDir = Path.Combine(dbcDir, "patched");
        Directory.CreateDirectory(patchedDir);
        string outSpell = Path.Combine(patchedDir, "Spell.dbc");
        string outSla = Path.Combine(patchedDir, "SkillLineAbility.dbc");

        PatchSpell(spellPath, outSpell);
        PatchSkillLine(slaPath, outSla);

        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(mpqPath)));
        var files = new List<MpqFile>
        {
            new MpqFile(@"DBFilesClient\Spell.dbc", File.ReadAllBytes(outSpell)),
            new MpqFile(@"DBFilesClient\SkillLineAbility.dbc", File.ReadAllBytes(outSla)),
        };
        MpqWriter.Write(mpqPath, files);
        Console.WriteLine("Wrote " + Path.GetFullPath(mpqPath) + " (" + new FileInfo(mpqPath).Length + " bytes)");
        return 0;
    }

    static void PatchSpell(string src, string dest)
    {
        var dbc = Dbc.Load(src);
        int template = dbc.FindRecord(SpellIdField, (int)LavaBurstTemplateId);
        if (template < 0)
            throw new Exception("Spell.dbc has no 51505 Lava Burst");

        int gw = dbc.FindRecord(SpellIdField, (int)GhostWolfId);
        if (gw >= 0)
        {
            dbc.SetU32(gw, SpellCastTime, 1); // instant
            Console.WriteLine("Ghost Wolf (2645): CastingTimeIndex -> 1 (instant)");
        }

        foreach (Rank rank in Ranks)
        {
            int existing = dbc.FindRecord(SpellIdField, (int)rank.Id);
            int rec = existing >= 0 ? existing : dbc.CloneRecord(template);
            dbc.SetU32(rec, SpellIdField, rank.Id);
            dbc.SetU32(rec, SpellBaseLevel, rank.Level);
            dbc.SetU32(rec, SpellSpellLevel, rank.Level);
            dbc.SetI32(rec, SpellBasePoints1, rank.Min - 1);
            dbc.SetI32(rec, SpellDieSides1, rank.Max - rank.Min + 1);
            dbc.SetU32(rec, SpellManaPct, 10);
            dbc.SetU32(rec, SpellRank, dbc.AddString(rank.Text));
            Console.WriteLine(string.Format("  Spell {0} {1} level {2} {3}-{4}",
                rank.Id, rank.Text, rank.Level, rank.Min, rank.Max));
        }

        dbc.Save(dest);
        Console.WriteLine("Wrote " + dest + " (" + dbc.RecordCount + " records)");
    }

    static void PatchSkillLine(string src, string dest)
    {
        var dbc = Dbc.Load(src);
        const int spellField = 2; // 3.3.5 SkillLineAbility.Spell
        int template = dbc.FindRecord(spellField, (int)LavaBurstTemplateId);
        if (template < 0)
        {
            Console.WriteLine("SkillLineAbility.dbc has no 51505 row; skip");
            File.Copy(src, dest, true);
            return;
        }

        uint nextId = 800001;
        foreach (Rank rank in Ranks)
        {
            if (dbc.FindRecord(spellField, (int)rank.Id) >= 0)
            {
                Console.WriteLine("  SkillLineAbility already has " + rank.Id);
                continue;
            }
            int rec = dbc.CloneRecord(template);
            dbc.SetU32(rec, 0, nextId++);
            dbc.SetU32(rec, spellField, rank.Id);
            Console.WriteLine("  SkillLineAbility " + (nextId - 1) + " -> spell " + rank.Id);
        }

        dbc.Save(dest);
        Console.WriteLine("Wrote " + dest + " (" + dbc.RecordCount + " records)");
    }

    struct Rank
    {
        public uint Id;
        public uint Level;
        public int Min;
        public int Max;
        public string Text;
        public Rank(uint id, uint level, int min, int max, string text)
        {
            Id = id; Level = level; Min = min; Max = max; Text = text;
        }
    }
}

internal sealed class Dbc
{
    public int FieldCount;
    public int RecordSize;
    public List<byte[]> Records = new List<byte[]>();
    public List<byte> Strings = new List<byte>();

    public int RecordCount { get { return Records.Count; } }

    public static Dbc Load(string path)
    {
        byte[] data = File.ReadAllBytes(path);
        if (data.Length < 20 || Encoding.ASCII.GetString(data, 0, 4) != "WDBC")
            throw new Exception(path + " is not a WDBC file");

        int recCount = BitConverter.ToInt32(data, 4);
        int fieldCount = BitConverter.ToInt32(data, 8);
        int recSize = BitConverter.ToInt32(data, 12);
        int strSize = BitConverter.ToInt32(data, 16);

        var dbc = new Dbc { FieldCount = fieldCount, RecordSize = recSize };
        int pos = 20;
        for (int i = 0; i < recCount; i++)
        {
            byte[] rec = new byte[recSize];
            Buffer.BlockCopy(data, pos, rec, 0, recSize);
            dbc.Records.Add(rec);
            pos += recSize;
        }
        for (int i = 0; i < strSize; i++)
            dbc.Strings.Add(data[pos + i]);
        if (dbc.Strings.Count == 0)
            dbc.Strings.Add(0);
        return dbc;
    }

    public int FindRecord(int field, int value)
    {
        for (int i = 0; i < Records.Count; i++)
        {
            if (GetI32(i, field) == value)
                return i;
        }
        return -1;
    }

    public int CloneRecord(int index)
    {
        byte[] copy = (byte[])Records[index].Clone();
        Records.Add(copy);
        return Records.Count - 1;
    }

    public int GetI32(int rec, int field)
    {
        return BitConverter.ToInt32(Records[rec], field * 4);
    }

    public void SetI32(int rec, int field, int value)
    {
        byte[] b = BitConverter.GetBytes(value);
        Buffer.BlockCopy(b, 0, Records[rec], field * 4, 4);
    }

    public void SetU32(int rec, int field, uint value)
    {
        byte[] b = BitConverter.GetBytes(value);
        Buffer.BlockCopy(b, 0, Records[rec], field * 4, 4);
    }

    public uint AddString(string text)
    {
        byte[] raw = Encoding.UTF8.GetBytes(text);
        uint offset = (uint)Strings.Count;
        Strings.AddRange(raw);
        Strings.Add(0);
        return offset;
    }

    public void Save(string path)
    {
        using (var fs = File.Create(path))
        using (var bw = new BinaryWriter(fs))
        {
            bw.Write(Encoding.ASCII.GetBytes("WDBC"));
            bw.Write(Records.Count);
            bw.Write(FieldCount);
            bw.Write(RecordSize);
            bw.Write(Strings.Count);
            foreach (byte[] rec in Records)
                bw.Write(rec);
            bw.Write(Strings.ToArray());
        }
    }
}

internal sealed class MpqFile
{
    public string Name;
    public byte[] Data;
    public MpqFile(string name, byte[] data) { Name = name; Data = data; }
}

internal static class MpqWriter
{
    const uint HashTableIndex = 0;
    const uint HashNameA = 1;
    const uint HashNameB = 2;
    const uint HashFileKey = 3;
    const uint FileExists = 0x80000000;
    const uint FileSingleUnit = 0x01000000;
    const uint HashEmpty = 0xFFFFFFFF;

    static readonly uint[] CryptTable = BuildCryptTable();

    public static void Write(string path, List<MpqFile> files)
    {
        var list = new List<MpqFile>(files);
        var names = new StringBuilder();
        foreach (MpqFile f in files)
            names.Append(f.Name).Append('\n');
        names.Append("(listfile)\n");
        list.Add(new MpqFile("(listfile)", Encoding.ASCII.GetBytes(names.ToString())));

        int hashSize = 16;
        while (hashSize < list.Count * 4)
            hashSize *= 2;

        var blocks = new Block[list.Count];
        var hashes = new Hash[hashSize];
        for (int i = 0; i < hashSize; i++)
        {
            hashes[i].Name1 = 0xFFFFFFFF;
            hashes[i].Name2 = 0xFFFFFFFF;
            hashes[i].Locale = 0xFFFF;
            hashes[i].Platform = 0xFFFF;
            hashes[i].BlockIndex = HashEmpty;
        }

        uint dataPos = 32;
        using (var ms = new MemoryStream())
        {
            // placeholder header
            ms.Write(new byte[32], 0, 32);

            for (int i = 0; i < list.Count; i++)
            {
                byte[] data = list[i].Data;
                blocks[i].FilePos = dataPos;
                blocks[i].CompressedSize = (uint)data.Length;
                blocks[i].FileSize = (uint)data.Length;
                blocks[i].Flags = FileExists | FileSingleUnit;
                ms.Write(data, 0, data.Length);
                dataPos += (uint)data.Length;

                InsertHash(hashes, list[i].Name, (uint)i);
            }

            uint hashOffset = dataPos;
            byte[] hashBytes = SerializeHashes(hashes);
            Encrypt(hashBytes, HashString("(hash table)", HashFileKey));
            ms.Write(hashBytes, 0, hashBytes.Length);

            uint blockOffset = hashOffset + (uint)hashBytes.Length;
            byte[] blockBytes = SerializeBlocks(blocks);
            Encrypt(blockBytes, HashString("(block table)", HashFileKey));
            ms.Write(blockBytes, 0, blockBytes.Length);

            uint archiveSize = (uint)ms.Length;
            ms.Position = 0;
            var hdr = new BinaryWriter(ms);
            hdr.Write(Encoding.ASCII.GetBytes("MPQ\x1A"));
            hdr.Write(32u);                 // header size
            hdr.Write(archiveSize);
            hdr.Write((ushort)0);           // format version 0
            hdr.Write((ushort)3);           // block size 512<<3 = 4096
            hdr.Write(hashOffset);
            hdr.Write(blockOffset);
            hdr.Write((uint)hashSize);
            hdr.Write((uint)blocks.Length);

            File.WriteAllBytes(path, ms.ToArray());
        }
    }

    static void InsertHash(Hash[] table, string name, uint blockIndex)
    {
        uint index = HashString(name, HashTableIndex) & ((uint)table.Length - 1);
        uint name1 = HashString(name, HashNameA);
        uint name2 = HashString(name, HashNameB);
        for (int i = 0; i < table.Length; i++)
        {
            uint slot = (index + (uint)i) & ((uint)table.Length - 1);
            if (table[slot].BlockIndex == HashEmpty)
            {
                table[slot].Name1 = name1;
                table[slot].Name2 = name2;
                table[slot].Locale = 0;
                table[slot].Platform = 0;
                table[slot].BlockIndex = blockIndex;
                return;
            }
        }
        throw new Exception("hash table full");
    }

    static byte[] SerializeHashes(Hash[] table)
    {
        var ms = new MemoryStream(table.Length * 16);
        var bw = new BinaryWriter(ms);
        foreach (Hash h in table)
        {
            bw.Write(h.Name1);
            bw.Write(h.Name2);
            bw.Write(h.Locale);
            bw.Write(h.Platform);
            bw.Write(h.BlockIndex);
        }
        return ms.ToArray();
    }

    static byte[] SerializeBlocks(Block[] table)
    {
        var ms = new MemoryStream(table.Length * 16);
        var bw = new BinaryWriter(ms);
        foreach (Block b in table)
        {
            bw.Write(b.FilePos);
            bw.Write(b.CompressedSize);
            bw.Write(b.FileSize);
            bw.Write(b.Flags);
        }
        return ms.ToArray();
    }

    static uint[] BuildCryptTable()
    {
        var table = new uint[0x500];
        uint seed = 0x00100001;
        for (uint index1 = 0; index1 < 0x100; index1++)
        {
            uint index2 = index1;
            for (int i = 0; i < 5; i++, index2 += 0x100)
            {
                seed = (seed * 125 + 3) % 0x2AAAAB;
                uint temp1 = (seed & 0xFFFF) << 16;
                seed = (seed * 125 + 3) % 0x2AAAAB;
                uint temp2 = seed & 0xFFFF;
                table[index2] = temp1 | temp2;
            }
        }
        return table;
    }

    static uint HashString(string text, uint type)
    {
        uint seed1 = 0x7FED7FED;
        uint seed2 = 0xEEEEEEEE;
        foreach (char raw in text)
        {
            char ch = raw == '/' ? '\\' : char.ToUpperInvariant(raw);
            uint c = ch;
            seed1 = CryptTable[(type << 8) + c] ^ (seed1 + seed2);
            seed2 = c + seed1 + seed2 + (seed2 << 5) + 3;
        }
        return seed1;
    }

    static void Encrypt(byte[] data, uint key)
    {
        uint seed = 0xEEEEEEEE;
        for (int i = 0; i + 4 <= data.Length; i += 4)
        {
            seed += CryptTable[0x400 + (key & 0xFF)];
            uint val = BitConverter.ToUInt32(data, i);
            uint ch = val ^ (key + seed);
            byte[] b = BitConverter.GetBytes(ch);
            Buffer.BlockCopy(b, 0, data, i, 4);
            key = ((~key << 21) + 0x11111111) | (key >> 11);
            seed = ch + seed + (seed << 5) + 3;
        }
    }

    struct Hash
    {
        public uint Name1;
        public uint Name2;
        public ushort Locale;
        public ushort Platform;
        public uint BlockIndex;
    }

    struct Block
    {
        public uint FilePos;
        public uint CompressedSize;
        public uint FileSize;
        public uint Flags;
    }
}
