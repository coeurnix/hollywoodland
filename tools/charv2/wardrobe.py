"""The v2 wardrobe: garment bases (MakeHuman meshes fitted once per sex) and the 33 looks that dress them.

A base is one MHCLO garment, fitted to a sex's body and split into slot pieces. A look picks, per sex and slot, a piece
of a base; its texture is the base's own texture map repainted for that look by FLUX 3 edit (tools/charv2/retex.py),
from the base's piece template filled with the look's palette. Textures are per (base, look), so the coat and trousers of
a suit worn together always match, and each piece still stands alone when slots are mixed.

Slot pieces: shirt, coat (outer layer: suit jackets, coats, dress bodices; it carries its own visible shirt front),
trousers (or skirt), shoes, gloves. Belts, wrist pieces and hats come from other generators (procedural / Rodin).
"""

# base id: (sex, mhclo stem, {piece: slot}, covers per piece, prompt template naming the pieces in its texture map)
# split: a garment listed with several pieces is cut by connected part (export_v2.split_parts: below the hips =
# trousers, the rest coat).
Z_TORSO = ['chest', 'belly', 'upperarm', 'forearm']
Z_LEGS = ['hips', 'thigh', 'calf']
BASES = {
    # ---- men
    'm_shirt_tie': ('Male', 'elvs_male_shirt_tie_tucked1', {'shirt': 'shirt'}, {'shirt': Z_TORSO}, 'a {shirt} shirt with a {accent} tie'),
    'm_shirt_open': ('Male', 'elvs_male_shirt_untucked_bd1', {'shirt': 'shirt'}, {'shirt': Z_TORSO}, 'a {shirt} work shirt'),
    'm_henley': ('Male', 'namuhekam_male_polo_shirt', {'shirt': 'shirt'}, {'shirt': ['chest', 'belly', 'upperarm']}, 'a {shirt} henley undershirt'),
    'm_sweater': ('Male', 'mindfront_knitted_sweater_01', {'shirt': 'shirt'}, {'shirt': Z_TORSO}, 'a {fabric} knitted jersey'),
    'm_tunic': ('Male', 'rehmanpolanski_viking_tunic', {'shirt': 'shirt'}, {'shirt': Z_TORSO + ['hips']}, 'a {fabric} tunic over {metal} mail'),
    'm_suit3': ('Male', 'toigo_male_suit_3', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                '{fabric} jacket and trousers, a {shirt} shirt and a {accent} tie, {metal} buttons'),
    'm_suit_db': ('Male', 'toigo_male_double-breasted_suit', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                  '{fabric} double-breasted jacket and trousers, a {shirt} shirt and a {accent} tie, {metal} buttons'),
    'm_suit_tie': ('Male', 'toigo_male_suit_tie_and_jacket', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                   '{fabric} jacket and trousers, a {shirt} shirt and a {accent} tie'),
    'm_dinner': ('Male', 'toigo_suit_with_dinner_jacket', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                 'a {fabric} dinner jacket with {accent} lapels, {leather} trousers, a {shirt} shirt and bow tie'),
    'm_bowtie': ('Male', 'toigo_suit_with_jacket_and_bowtie', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                 'a {fabric} evening jacket and trousers, a {shirt} shirt and a {accent} bow tie'),
    'm_jacket': ('Male', 'male_casualsuit05', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                 'a {fabric} work jacket with pockets, a {shirt} shirt underneath, {leather} trousers'),
    'm_overalls': ('Male', 'male_worksuit01', {'trousers': 'trousers'}, {'trousers': ['chest', 'belly'] + Z_LEGS},
                   '{fabric} bib overalls with {metal} buckles over a {shirt} shirt'),
    'm_longcoat': ('Male', 'mindfront_cardigan_long_open_front', {'coat': 'coat'}, {'coat': Z_TORSO + ['hips', 'thigh']},
                   'a long {fabric} coat with {accent} trim'),
    'm_wool': ('Male', 'toigo_wool_pants', {'trousers': 'trousers'}, {'trousers': Z_LEGS}, '{fabric} trousers'),
    'm_slacks': ('Male', 'mindfront_male_trousers_1', {'trousers': 'trousers'}, {'trousers': Z_LEGS}, '{fabric} trousers'),
    'm_cargo': ('Male', 'cortu_cargo_pants', {'trousers': 'trousers'}, {'trousers': Z_LEGS}, '{fabric} work trousers with pockets'),
    'm_breeches': ('Male', 'rehmanpolanski_viking_pants', {'trousers': 'trousers'}, {'trousers': Z_LEGS}, '{fabric} riding breeches'),
    'm_oxford': ('Male', 'mindfront_shoes_oxford_male', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} oxford shoes'),
    'm_saddle': ('Male', 'callharvey3d_harvey_saddleshoes1', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} and {shirt} saddle shoes'),
    'm_ankle': ('Male', 'toigo_ankle_boots_male', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} ankle boots'),
    'm_workboot': ('Male', 'maciekg_leather_boots', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} hobnail work boots'),
    'm_tallboot': ('Male', 'punkduck_riding_boots', {'shoes': 'shoes'}, {'shoes': ['foot', 'calf']}, 'tall {leather} riding boots'),
    'm_trenchboot': ('Male', 'punkduck_medieval_boots', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} trench boots'),
    'm_gloves_s': ('Male', 'toigo_gloves_short', {'gloves': 'gloves'}, {'gloves': ['hand']}, '{leather} gloves'),
    'm_gloves_m': ('Male', 'toigo_gloves_medium', {'gloves': 'gloves'}, {'gloves': ['hand', 'forearm']}, '{leather} gauntlet gloves'),
    # ---- women
    'f_blouse': ('Female', 'punkduck_lace_up_blouse', {'shirt': 'shirt'}, {'shirt': Z_TORSO}, 'a {shirt} blouse'),
    'f_peasant': ('Female', 'elvs_ruffle_sleeve_peasant_blouse_1', {'shirt': 'shirt'}, {'shirt': ['chest', 'belly', 'upperarm']}, 'a {shirt} blouse'),
    'f_sweater': ('Female', 'janexx_old_female_sweater', {'shirt': 'shirt'}, {'shirt': Z_TORSO}, 'a {fabric} knitted jersey'),
    'f_suit': ('Female', 'toigo_female_suit', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': ['hips', 'thigh']},
               'a fitted {fabric} jacket and pencil skirt, a {shirt} blouse, {metal} buttons'),
    'f_suit_db': ('Female', 'toigo_female_double-breasted_suit', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                  'a fitted {fabric} double-breasted jacket and trousers, a {shirt} blouse and a {accent} tie'),
    'f_gown': ('Female', 'toigo_halter_dress_with_fluted_skirt', {'coat': 'coat'}, {'coat': ['chest', 'belly'] + Z_LEGS},
               'a floor-length {fabric} bias-cut evening gown with {accent} trim'),
    'f_midi': ('Female', 'toigo_halter_dress_midi', {'coat': 'coat'}, {'coat': ['chest', 'belly', 'hips', 'thigh']},
               'a {fabric} halter dress with {accent} trim'),
    'f_day': ('Female', 'toigo_camisole_dress_with_full_skirt', {'coat': 'coat'}, {'coat': ['chest', 'belly', 'hips', 'thigh']},
              'a {fabric} day dress with a full skirt and {accent} trim'),
    'f_longcoat': ('Female', 'mindfront_cardigan_long_open_front', {'coat': 'coat'}, {'coat': Z_TORSO + ['hips', 'thigh']}, 'a long {fabric} coat with {accent} trim'),
    'f_jacket': ('Female', 'female_casualsuit02', {'coat': 'coat', 'trousers': 'trousers'}, {'coat': Z_TORSO, 'trousers': Z_LEGS},
                 'a fitted {fabric} jacket, {leather} trousers'),
    'f_skirt': ('Female', 'toigo_long_full_skirt', {'trousers': 'trousers'}, {'trousers': ['hips', 'thigh']}, 'a long {fabric} skirt'),
    'f_trousers': ('Female', 'mindfront_female_trousers_1', {'trousers': 'trousers'}, {'trousers': Z_LEGS}, 'fitted {fabric} trousers'),
    'f_jodhpurs': ('Female', 'punkduck_female_tight_jeans', {'trousers': 'trousers'}, {'trousers': Z_LEGS}, 'fitted {fabric} jodhpurs'),
    'f_maryjane': ('Female', 'dressupdoc_maryjane1', {'shoes': 'shoes'}, {'shoes': []}, '{leather} Mary Jane shoes'),
    'f_oxford': ('Female', 'mindfront_shoes_oxford_female', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} oxford shoes'),
    'f_heels': ('Female', 'mindfront_shoes_monk_strap_female', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} heeled shoes'),
    'f_ankle': ('Female', 'toigo_ankle_boots_female', {'shoes': 'shoes'}, {'shoes': ['foot']}, '{leather} ankle boots'),
    'f_tallboot': ('Female', 'punkduck_female_knee_boots', {'shoes': 'shoes'}, {'shoes': ['foot', 'calf']}, 'tall {leather} boots'),
    'f_gloves_s': ('Female', 'toigo_gloves_short', {'gloves': 'gloves'}, {'gloves': ['hand']}, '{leather} gloves'),
    'f_gloves_l': ('Female', 'toigo_gloves_long', {'gloves': 'gloves'}, {'gloves': ['hand', 'forearm']}, 'long {leather} opera gloves'),
}

# look: palette (fabric, shirt, accent, leather, metal) and, per sex, {slot: base}
P = lambda fabric, shirt, accent, leather, metal: dict(fabric=fabric, shirt=shirt, accent=accent, leather=leather, metal=metal)
LOOKS = {
    'Starter': (P('worn brown wool', 'faded cream cotton', 'brown', 'scuffed brown leather', 'dull brass'),
                {'Male': dict(shirt='m_shirt_open', trousers='m_wool', shoes='m_workboot', coat=None, gloves=None),
                 'Female': dict(shirt='f_blouse', trousers='f_skirt', shoes='f_oxford', coat=None, gloves=None)}),
    'C1': (P('grey herringbone wool', 'blue-and-white striped cotton', 'charcoal', 'brown leather', 'nickel'),
           {'Male': dict(shirt='m_shirt_open', coat='m_jacket', trousers='m_overalls', shoes='m_saddle', gloves='m_gloves_s'),
            'Female': dict(shirt='f_peasant', coat='f_day', trousers='f_skirt', shoes='f_maryjane', gloves='f_gloves_s')}),
    'C2': (P('chocolate-brown tweed with a faint herringbone', 'cream cotton', 'bottle green with small gold dots', 'tan leather', 'horn'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_suit_tie', trousers='m_suit_tie', shoes='m_oxford', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_day', trousers='f_skirt', shoes='f_oxford', gloves='f_gloves_s')}),
    'C3': (P('charcoal pinstripe worsted wool', 'crisp white cotton', 'deep burgundy silk', 'polished black leather', 'silver'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_suit3', trousers='m_suit3', shoes='m_oxford', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_suit', trousers='f_suit', shoes='f_heels', gloves='f_gloves_s')}),
    'C4': (P('black satin and fine black wool', 'white pique cotton', 'white silk', 'black patent leather', 'gold'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_bowtie', trousers='m_bowtie', shoes='m_oxford', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_gown', trousers='f_skirt', shoes='f_heels', gloves='f_gloves_l')}),
    'C5': (P('ivory white wool with art-deco gold trim', 'white silk', 'gold', 'black patent leather', 'gold'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_dinner', trousers='m_dinner', shoes='m_oxford', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_gown', trousers='f_skirt', shoes='f_heels', gloves='f_gloves_l')}),
    'C6': (P('black wool inlaid with glossy green-black insect chitin plates with violet iridescent edges', 'ivory silk', 'deep violet silk',
             'black chitin-shell leather', 'violet-black chitin'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_suit3', trousers='m_suit3', shoes='m_oxford', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_gown', trousers='f_skirt', shoes='f_heels', gloves='f_gloves_l')}),
    'L1': (P('brown horsehide flight leather with cream sheepskin', 'khaki cotton', 'cream sheepskin', 'brown leather', 'brass'),
           {'Male': dict(shirt='m_shirt_open', coat='m_jacket', trousers='m_cargo', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_ankle', gloves='f_gloves_s')}),
    'L2': (P('dark russet horsehide leather', 'tan cotton', 'saddle brown', 'oiled brown leather', 'brass'),
           {'Male': dict(shirt='m_shirt_open', coat='m_jacket', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'L3': (P('tooled tan suede with fringe', 'red gingham cotton', 'red bandana cotton', 'tooled brown leather', 'silver conchos'),
           {'Male': dict(shirt='m_shirt_open', coat='m_jacket', trousers='m_slacks', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_peasant', coat='f_jacket', trousers='f_skirt', shoes='f_tallboot', gloves='f_gloves_s')}),
    'L4': (P('black and tan stunt-rider leather with laced panels', 'grey cotton', 'tan', 'black leather', 'steel'),
           {'Male': dict(shirt='m_henley', coat='m_jacket', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'L5': (P('dark brown leather lined with red-and-black Pendleton plaid wool', 'khaki cotton', 'red plaid', 'brown leather', 'brass'),
           {'Male': dict(shirt='m_shirt_open', coat='m_longcoat', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_longcoat', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'L6': (P('dark leather studded with glossy green-black insect chitin plates with amber rims', 'black cotton', 'amber', 'black chitin-capped leather',
             'amber-rimmed chitin'),
           {'Male': dict(shirt='m_henley', coat='m_jacket', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'P1': (P('brown duck canvas', 'blue chambray cotton', 'faded denim', 'worn brown leather', 'copper rivets'),
           {'Male': dict(shirt='m_shirt_open', coat='m_jacket', trousers='m_overalls', shoes='m_workboot', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'P2': (P('padded olive canvas', 'grey wool', 'orange', 'heavy brown leather', 'steel'),
           {'Male': dict(shirt='m_henley', coat='m_jacket', trousers='m_cargo', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_sweater', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'P3': (P('quilted maroon and gold football canvas', 'maroon jersey with a gold stripe', 'gold', 'tan leather', 'brass'),
           {'Male': dict(shirt='m_sweater', coat='m_jacket', trousers='m_cargo', shoes='m_workboot', gloves='m_gloves_s'),
            'Female': dict(shirt='f_sweater', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'P4': (P('quilted khaki aviator padding', 'navy wool', 'sheepskin', 'brown leather', 'brass'),
           {'Male': dict(shirt='m_sweater', coat='m_jacket', trousers='m_wool', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_sweater', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'P5': (P('scorched heavy grey canvas', 'denim', 'rust', 'thick brown welding leather', 'blackened steel'),
           {'Male': dict(shirt='m_henley', coat='m_jacket', trousers='m_overalls', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'P6': (P('quilted padding reinforced with glossy green-black insect chitin panels with violet edges', 'black cotton', 'violet', 'chitin-capped leather', 'chitin'),
           {'Male': dict(shirt='m_sweater', coat='m_jacket', trousers='m_cargo', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_sweater', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'S1': (P('olive drab wool with a riveted grey steel vest', 'olive wool', 'khaki webbing', 'brown leather with wool puttees', 'grey steel'),
           {'Male': dict(shirt='m_shirt_open', coat='m_jacket', trousers='m_breeches', shoes='m_trenchboot', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_ankle', gloves='f_gloves_s')}),
    'S2': (P('khaki gabardine with riveted steel plate inserts', 'olive wool', 'steel', 'brown leather', 'riveted steel'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_longcoat', trousers='m_breeches', shoes='m_trenchboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_longcoat', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'S3': (P('grey wool with steel mail panels', 'steel chainmail', 'navy', 'black leather with steel plates', 'polished steel'),
           {'Male': dict(shirt='m_tunic', coat='m_jacket', trousers='m_breeches', shoes='m_trenchboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'S4': (P('polished studio-forged steel plates with art-deco stepped trim over khaki', 'khaki cotton', 'brass', 'steel-plated leather', 'polished steel'),
           {'Male': dict(shirt='m_tunic', coat='m_jacket', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_l')}),
    'S5': (P('blackened steel plates with brass art-deco sunburst trim over dark navy', 'dark navy wool', 'brass', 'blackened leather', 'brass'),
           {'Male': dict(shirt='m_tunic', coat='m_jacket', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_l')}),
    'S6': (P('glossy green-black insect chitin plates with amber rims', 'black cloth', 'amber', 'chitin', 'chitin'),
           {'Male': dict(shirt='m_tunic', coat='m_jacket', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_l')}),
    'E1': (P('olive officer barathea wool with gold braid and medals', 'khaki cotton', 'gold braid', 'polished oxblood leather', 'brass and steel'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_suit_db', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_suit_db', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'E2': (P('red and white racing leather with white stars', 'white silk', 'red', 'white leather', 'chrome'),
           {'Male': dict(shirt='m_henley', coat='m_jacket', trousers='m_breeches', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'E3': (P('weathered black duster canvas', 'charcoal wool waistcoat over a white shirt', 'silver star badge', 'tooled black leather', 'silver'),
           {'Male': dict(shirt='m_shirt_tie', coat='m_longcoat', trousers='m_slacks', shoes='m_tallboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_longcoat', trousers='f_jodhpurs', shoes='f_tallboot', gloves='f_gloves_s')}),
    'E4': (P('padded slate-blue electrician canvas with coiled black cables', 'grey cotton', 'safety orange', 'black rubber', 'copper'),
           {'Male': dict(shirt='m_henley', coat='m_jacket', trousers='m_overalls', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'E5': (P('scorched grey asbestos fire cloth with brass buckles', 'charcoal cotton', 'flame orange', 'padded brown leather', 'brass'),
           {'Male': dict(shirt='m_henley', coat='m_longcoat', trousers='m_cargo', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_longcoat', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
    'E6': (P('crisp white cotton with a red cross', 'white cotton', 'red cross', 'white leather', 'silver'),
           {'Male': dict(shirt='m_shirt_open', coat='m_jacket', trousers='m_slacks', shoes='m_oxford', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_day', trousers='f_skirt', shoes='f_oxford', gloves='f_gloves_s')}),
    'E7': (P('black wool frock cloth with a purple stole', 'white cotton with a clerical collar', 'purple', 'black leather', 'brass'),
           {'Male': dict(shirt='m_shirt_open', coat='m_longcoat', trousers='m_slacks', shoes='m_ankle', gloves='m_gloves_s'),
            'Female': dict(shirt='f_blouse', coat='f_longcoat', trousers='f_skirt', shoes='f_ankle', gloves='f_gloves_s')}),
    'E8': (P('tan canvas coveralls with many pockets', 'grey cotton', 'gaffer-tape silver', 'worn brown leather', 'steel'),
           {'Male': dict(shirt='m_henley', coat='m_jacket', trousers='m_overalls', shoes='m_workboot', gloves='m_gloves_m'),
            'Female': dict(shirt='f_blouse', coat='f_jacket', trousers='f_trousers', shoes='f_ankle', gloves='f_gloves_s')}),
}


def check():
    for look, (pal, sexes) in LOOKS.items():
        for sex, slots in sexes.items():
            for slot, base in slots.items():
                if base is None:
                    continue
                assert base in BASES, f'{look} {sex} {slot}: unknown base {base}'
                b = BASES[base]
                assert b[0] == sex, f'{look} {sex} {slot}: {base} is for {b[0]}'
                assert slot in b[2].values(), f'{look} {sex} {slot}: {base} has no {slot} piece ({b[2]})'
    return True


if __name__ == '__main__':
    check()
    used = {(b) for _, (p, s) in LOOKS.items() for sl in s.values() for b in sl.values() if b}
    print('ok', len(LOOKS), 'looks', len(used), 'bases used of', len(BASES), 'textures', sum(1 for _ in {(b, l) for l, (p, s) in LOOKS.items() for sl in s.values() for b in sl.values() if b}))

# hair styles per sex: (MHCLO stem, label); exported as SKV2_<Sex>_<stem>_hair (build_wardrobe.py --hair)
HAIR = {'Male': [('short02', 'Short')], 'Female': [('toigo_curled_under_bob', 'Curled Bob')]}
