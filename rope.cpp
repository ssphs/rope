#include "rope.h"
#include <cassert>

#if defined(MAXMM_ROPE_DEBUG)
  #include <iostream>
  #define ASSERT(x) assert(x)
  #define ASSERT_INVARIANT assert_invariant(*this);

#else
  #define ASSERT(x) 
  #define ASSERT_INVARIANT

#endif

namespace maxmm {

rope::rope(std::string const& s) 
: tag_(tag::STRING), 
  height_(1), 
  size_(s.size()),
  parent_(nullptr) { 
 
  new (&string_) std::string(s);
  ASSERT_INVARIANT;
}

rope::rope(std::string&& s) 
: tag_(tag::STRING), 
  height_(1), 
  size_(s.size()),
  parent_(nullptr) { 

  new (&string_) std::string(std::move(s));
  ASSERT_INVARIANT;
}

rope::rope(rope&& lhs, 
           rope&& rhs) 
: tag_(tag::APPEND), 
  height_(std::max(lhs.height_, rhs.height_) + 1), 
  size_(lhs.size_ + rhs.size_), 
  parent_(nullptr) {

  new (&append_) rope::append(std::move(lhs), 
                              std::move(rhs), 
                              this);
  ASSERT_INVARIANT;
}
  
rope::rope(rope&& copy) {
    
  using std::string; 

  switch(copy.tag_) {
  case tag::STRING: new (&string_) string(std::move(copy.string_));break;
  case tag::APPEND: new (&append_) append(std::move(copy.append_), this);break;
  }
  
  tag_    = copy.tag_;
  size_   = copy.size_;
  height_ = copy.height_;
  parent_ = copy.parent_;

  // -- Note --
  //
  // It's intentional that we left the [copy] unmodified besides 
  // moving it's [string_] or [append_] data members. 
  //
  // This pmaxmmrty is needed and will be leveraged in the 
  // ::append_string() member function.

  ASSERT_INVARIANT;
}

rope::~rope() {
  using std::string; 
  switch(tag_) {
  case tag::STRING: string_.~string(); break;
  case tag::APPEND: append_.~append(); break;
  }
}

std::size_t rope::size() const {
  ASSERT_INVARIANT;
  return size_;
}

std::size_t rope::height() const {
  ASSERT_INVARIANT;
  return height_;
}

char& rope::operator[](std::size_t index) {
  ASSERT_INVARIANT;
  switch(tag_) {

  case tag::STRING: {
    return string_[index];
  } break;

  case tag::APPEND: {
    std::size_t lhssize_ = append_.lhs_->size_; 
    if(index >= lhssize_) {
      return append_.rhs_->operator[](index - lhssize_);
    }
    else {
      return append_.lhs_->operator[](index);
    }
  } break;

  } // switch
}

char rope::operator[](std::size_t index) const {
  ASSERT_INVARIANT;

  rope& self = const_cast<rope&>(*this); 
  return self[index];
}

rope& rope::append_string(std::string&& s) {
  ASSERT_INVARIANT;

  std::size_t ssize_ = s.size();

  switch(tag_) {

  case tag::STRING:{
    
    append a(std::move(*this), 
             rope(std::move(s)), 
             this);
    
    using std::string;
    string_.~string();
    new (&append_) append(std::move(a), this); 
    tag_    = tag::APPEND;

  } break;
  
  case tag::APPEND:{
    
    if(append_.rhs_->height_ >= append_.lhs_->height_) {
      // rebalance on the other side.
      
      append a(std::move(*this), 
               rope(std::move(s)), 
               this);
    
      append_.~append();
      new (&append_) append(std::move(a), this); 
      tag_    = tag::APPEND;
    }
    else {
      // append on the rhs which has room.
      append_.rhs_->append_string(std::move(s));
    }

  } break;

  }

  size_  += ssize_; 
  height_ = std::max(append_.rhs_->height_, append_.lhs_->height_) + 1; 

  // -- Note -- 
  //
  // The [parent_] is unchanged due to the pmaxmmrty of the 
  // move constructor [rope::(t&& copy)] which does not modify the data
  // members. So even if we called [std::move(this)] above, the 
  // [parent_] is still valid.

  return *this;
}

rope& rope::append_string(std::string const& rhs) {
  return this->append_string(std::string(rhs));
}

rope* rope::left_most_string() {
  ASSERT_INVARIANT;

  if(tag_ == tag::STRING) {
    return this;
  }
  else {
    return append_.lhs_->left_most_string();
  }
}
  
iterator rope::begin() {
  ASSERT_INVARIANT;

  if(size_ == 0) {
    return iterator();
  }
  else {
    return iterator(this->left_most_string(), 0);
  }
}

iterator rope::end() {
  ASSERT_INVARIANT;

  return iterator();
}

const_iterator rope::begin() const {
  ASSERT_INVARIANT;

  if(size_ == 0) {
    return const_iterator();
  }
  else {
    rope* self = const_cast<rope*>(this);
    return const_iterator(self->left_most_string(), 0);
  }
}

const_iterator rope::end() const {
  ASSERT_INVARIANT;

  return const_iterator();
}

rope::append::append(rope&& lhs, 
                     rope&& rhs, 
                     rope*   self)
: lhs_(new rope(std::move(lhs)))
 ,rhs_(new rope(std::move(rhs))) { 

  lhs_->parent_ = self;
  rhs_->parent_ = self;
}

rope::append::append(append&& copy, rope* self)
: lhs_(std::move(copy.lhs_)), 
  rhs_(std::move(copy.rhs_)) {

  lhs_->parent_ = self;
  rhs_->parent_ = self;
}

rope* rope::next_string_leaf() {
  ASSERT_INVARIANT;

  if(parent_ == nullptr) {
    // root node
    return nullptr;
  }

  ASSERT(parent_->tag_ == tag::APPEND); 
    // Check invariant.

  bool am_i_lhs = parent_->append_.lhs_.get() == this; 
  if(am_i_lhs) {
    // I am the lhs side node of my parent, 
    // the next one is my sibling the rhs. 
    return parent_->append_.rhs_.get (); 
  }

  // I am the rhs side of my parent, we need to 
  // go up a level and do this again to reach cousins/uncles/

  return parent_->next_string_leaf();
}
  
bool rope::check_invariant() const {

#if defined(MAXMM_ROPE_DEBUG)
  #define MAXMM_CHECK(x) \
    if(! (x)) {\
      std::cerr << "assertion failed: " << #x << std::endl;\
      return false;\
    }
#else
  #define MAXMM_CHECK(x)
#endif
  
  MAXMM_CHECK(size_ >= 0);

  if(tag_ == tag::STRING) {
    MAXMM_CHECK(size_ == string_.size());
    MAXMM_CHECK(height_ == 1);
  }

  if(tag_ == tag::APPEND) {
    
    // an append node is always created with 2 valid 
    // sub maxmms.
    MAXMM_CHECK(append_.lhs_.get() != nullptr);
    MAXMM_CHECK(append_.rhs_.get() != nullptr);

    rope& lhs = *append_.lhs_;
    rope& rhs = *append_.rhs_;

    // the size of an append is the sum of its sub maxmms' size.
    MAXMM_CHECK(size_ == lhs.size_ + rhs.size_);

    // the height of an append is one more than the maximum height of 
    // its sub maxmms.
    MAXMM_CHECK(height_ == std::max(lhs.height_, rhs.height_) + 1);

    // the sub maxmms parent_ field must point to the parent append maxmm.
    MAXMM_CHECK(lhs.parent_ == this);
    MAXMM_CHECK(rhs.parent_ == this);
  }

#undef MAXMM_CHECK 

  return true;
}
  
rope::assert_invariant::assert_invariant(rope const& rope)
: rope_(rope) {

  if(!rope_.check_invariant()) {
    assert(false);
  }
}

rope::assert_invariant::~assert_invariant() {
  assert(rope_.check_invariant());
}

// Iterators
// ---------

iterator::iterator() 
: _ptr(nullptr), 
  _index(0)
{}

iterator::iterator(rope* maxmm_ptr, 
                   std::size_t index)
: _ptr(maxmm_ptr), 
  _index(index)
{ 
  ASSERT(_ptr != nullptr); 
  ASSERT(_ptr->tag_ == rope::tag::STRING);
}

iterator::iterator(iterator const& copy)
: _ptr(copy._ptr), 
  _index(copy._index)
{
}

iterator::reference iterator::operator*() {
  ASSERT(_ptr != nullptr); 
  ASSERT(_ptr->tag_ == rope::tag::STRING);
  return _ptr->string_[_index];
}

iterator& iterator::operator++() {
  ASSERT(_ptr->tag_ == rope::tag::STRING);
  ++_index;
  if(_index == _ptr->string_.size()) {
      _ptr   = _ptr->next_string_leaf();
      _index = 0; 
  };
  return *this;
}

iterator iterator::operator++(int) {
  iterator tmp(*this); 
  ASSERT(_ptr->tag_ == rope::tag::STRING);
  ++_index;
  if(_index == _ptr->string_.size()) {
      _ptr   = _ptr->next_string_leaf();
      _index = 0; 
  };
  return tmp;
}

bool iterator::operator==(iterator const& rhs) const {
  return _ptr   == rhs._ptr && 
         _index == rhs._index;
}

bool iterator::operator!=(iterator const& rhs) const {
  return !this->operator==(rhs);
}

const_iterator::const_iterator() 
: _i() 
{}

const_iterator::const_iterator(rope const* ptr, 
                               std::size_t index)
: _i(const_cast<rope*>(ptr), index)
{ }

const_iterator::const_iterator(const_iterator const& copy)
: _i(copy._i)
{ }

const_iterator::value_type const_iterator::operator*() {
  return *_i;
}

const_iterator& const_iterator::operator++() {
  ++_i;
  return *this;
}

const_iterator const_iterator::operator++(int) {
    const_iterator tmp(*this); 
    ++_i;
    return tmp;
}

bool const_iterator::operator==(const_iterator const& rhs) const {
    return _i == rhs._i;
}

bool const_iterator::operator!=(const_iterator const& rhs) const {
    return _i != rhs._i;
}

} // namespace maxmm

#undef ASSERT
#undef ASSERT_INVARIANT
