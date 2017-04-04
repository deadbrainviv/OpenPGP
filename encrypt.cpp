#include "encrypt.h"

Packet::Ptr encrypt_data(const EncryptArgs & args,
                         const std::string & session_key,
                         std::string & error){

    if (!args.valid(error)){
        error += "Error: Bad argument.\n";
        return nullptr;
    }

    std::string to_encrypt;

    // if message is to be signed
    if (args.signer){
        const SignArgs signargs(*(args.signer), args.passphrase, 4, args.hash);
        PGPMessage signed_message = sign_binary(signargs, args.filename, args.data, args.comp, error);
        if (!signed_message.meaningful(error)){
            error += "Error: Signing failure.\n";
            return nullptr;
        }

        to_encrypt = signed_message.raw();
    }
    else{
        // put data in Literal Data Packet
        Tag11 tag11;
        tag11.set_format('b');
        tag11.set_filename(args.filename);
        tag11.set_time(0);
        tag11.set_literal(args.data);

        to_encrypt = tag11.write(Packet::Format::NEW);

        if (args.comp){
            // Compressed Data Packet (Tag 8)
            Tag8 tag8;
            tag8.set_comp(args.comp);
            tag8.set_data(to_encrypt); // put source data into compressed packet
            to_encrypt = tag8.write(Packet::Format::NEW);
        }
    }

    // generate prefix
    const std::size_t BS = Sym::BLOCK_LENGTH.at(args.sym);
    std::string prefix = unbinify(BBS().rand(BS));
    prefix += prefix.substr(prefix.size() - 2, 2);

    Packet::Ptr encrypted = nullptr;

    if (!args.mdc){
        // Symmetrically Encrypted Data Packet (Tag 9)
        Tag9 tag9;
        tag9.set_encrypted_data(use_OpenPGP_CFB_encrypt(args.sym, Packet::SYMMETRICALLY_ENCRYPTED_DATA, to_encrypt, session_key, prefix));
        encrypted = std::make_shared <Tag9> (tag9);
    }
    else{
        // Modification Detection Code Packet (Tag 19)
        Tag19 tag19;
        tag19.set_hash(use_hash(Hash::SHA1, prefix + to_encrypt + "\xd3\x14"));

        // Sym. Encrypted Integrity Protected Data Packet (Tag 18)
        // encrypt(compressed(literal_data_packet(plain text)) + MDC SHA1(20 octets))
        Tag18 tag18;
        tag18.set_protected_data(use_OpenPGP_CFB_encrypt(args.sym, Packet::SYM_ENCRYPTED_INTEGRITY_PROTECTED_DATA, to_encrypt + tag19.write(), session_key, prefix));
        encrypted = std::make_shared <Tag18> (tag18);
    }

    return encrypted;
}

PGPMessage encrypt_pka(const EncryptArgs & args,
                       const PGPPublicKey & pub,
                       std::string & error){
    BBS(static_cast <PGPMPI> (static_cast <unsigned int> (now()))); // seed just in case not seeded

    if (!args.valid(error)){
        error += "Error: Bad argument.\n";
        return PGPMessage();
    }

    if (!pub.meaningful(error)){
        error += "Error: Bad public key.\n";
        return PGPMessage();
    }

    // Check if key has been revoked
    const int rc = check_revoked(pub, error);
    if (rc == 1){
        error += "Error: Key " + hexlify(pub.keyid()) + " has been revoked. Nothing done.\n";
        return PGPMessage();
    }
    else if (rc == -1){
        error += "Error: check_revoked failed.\n";
        return PGPMessage();
    }

    Tag6::Ptr public_key = nullptr;
    for(Packet::Ptr const & p : pub.get_packets()){
        public_key = nullptr;
        if (Packet::is_key_packet(p -> get_tag())){
            public_key = std::static_pointer_cast <Tag6> (p);

            // make sure key has encrypting keys
            if (PKA::can_encrypt(public_key -> get_pka())){
                break;
            }
        }
    }

    if (!public_key){
        error += "Error: No encrypting key found.\n";
        return PGPMessage();
    }

    PKA::Values mpi = public_key -> get_mpi();
    Tag1::Ptr tag1 = std::make_shared <Tag1> ();
    tag1 -> set_keyid(public_key -> get_keyid());
    tag1 -> set_pka(public_key -> get_pka());

    // do calculations

    // generate session key
    const std::size_t key_len = Sym::KEY_LENGTH.at(args.sym);
    const std::string session_key = unbinify(BBS().rand(key_len));

    // get checksum of session key
    uint16_t sum = 0;
    for(char const c : session_key){
        sum += static_cast <unsigned char> (c);
    }

    std::string nibbles = mpitohex(mpi[0]);        // get hex representation of modulus
    nibbles += std::string(nibbles.size() & 1, 0); // get even number of nibbles
    PGPMPI m = hextompi(hexlify(EME_PKCS1v1_5_ENCODE(std::string(1, args.sym) + session_key + unhexlify(makehex(sum, 4)), nibbles.size() >> 1, error)));

    // encrypt m
    if ((public_key -> get_pka() == PKA::RSA_ENCRYPT_OR_SIGN) ||
        (public_key -> get_pka() == PKA::RSA_ENCRYPT_ONLY)){
        tag1 -> set_mpi({RSA_encrypt(m, mpi)});
    }
    else if (public_key -> get_pka() == PKA::ELGAMAL){
        tag1 -> set_mpi(ElGamal_encrypt(m, mpi));
    }

    // encrypt data and put it into a packet
    Packet::Ptr encrypted = encrypt_data(args, session_key, error);
    if (!encrypted){
        error += "Error: Failed to encrypt data.\n";
        return PGPMessage();
    }

    // write data to output container
    PGPMessage out;
    out.set_keys({std::make_pair("Version", "cc")});
    out.set_packets({tag1, encrypted});

    return out;
}

PGPMessage encrypt_sym(const EncryptArgs & args,
                       const std::string & passphrase,
                       const uint8_t key_hash,
                       std::string & error){
    BBS(static_cast <PGPMPI> (static_cast <unsigned int> (now()))); // seed just in case not seeded

    if (!args.valid(error)){
        error += "Error: Bad argument.\n";
        return PGPMessage();
    }

    // String to Key specifier for decrypting session key
    S2K3::Ptr s2k = std::make_shared <S2K3> ();
    s2k -> set_type(S2K::ITERATED_AND_SALTED_S2K);
    s2k -> set_hash(key_hash);
    s2k -> set_salt(unbinify(BBS().rand(64)));
    s2k -> set_count(96);

    // generate Symmetric-Key Encrypted Session Key Packets (Tag 3)
    Tag3::Ptr tag3 = std::make_shared <Tag3> ();
    tag3 -> set_version(4);
    tag3 -> set_sym(args.sym);
    tag3 -> set_s2k(s2k);

    // generate session key
    const std::string session_key = tag3 -> get_session_key(passphrase);

    // encrypt data
    Packet::Ptr encrypted = encrypt_data(args, session_key.substr(1, session_key.size() - 1), error);
    if (!encrypted){
        error += "Error: Failed to encrypt data.\n";
        return PGPMessage();
    }

    // write to output container
    PGPMessage out;
    out.set_keys({std::make_pair("Version", "cc")});
    out.set_packets({tag3, encrypted});

    return out;
}
