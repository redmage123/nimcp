"""
LLM Provider Abstraction - Dependency Injection for AI Models

WHAT: Abstract interface for LLM providers (Claude, GPT, local models, etc.)
WHY:  Enable swapping LLMs without changing core logic
HOW:  Strategy pattern with provider interface and concrete implementations

PRINCIPLES:
- Dependency Injection
- Strategy Pattern
- Interface Segregation
- Open/Closed Principle (open for extension, closed for modification)
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List, Dict, Optional
import os


@dataclass(frozen=True)
class LLMMessage:
    """
    Immutable message for LLM interaction

    WHAT: Represents a single message in LLM conversation
    WHY:  Provider-agnostic message format
    HOW:  Frozen dataclass with role and content
    """
    role: str  # "user" | "assistant" | "system"
    content: str


@dataclass(frozen=True)
class LLMResponse:
    """
    Immutable LLM response

    WHAT: Standardized response from any LLM provider
    WHY:  Consistent interface regardless of provider
    HOW:  Frozen dataclass with response data
    """
    text: str
    model: str
    tokens_used: Optional[int] = None
    finish_reason: Optional[str] = None
    raw_response: Optional[Dict] = None


class LLMProvider(ABC):
    """
    Abstract LLM provider interface

    WHAT: Contract for all LLM implementations
    WHY:  Dependency inversion - depend on abstractions not concretions
    HOW:  Abstract base class with pure virtual methods
    """

    @abstractmethod
    def generate(self, messages: List[LLMMessage], max_tokens: int = 4096) -> Optional[LLMResponse]:
        """
        Generate response from LLM

        WHAT: Send messages to LLM and get response
        WHY:  Core LLM interaction method
        HOW:  Provider-specific implementation

        PARAMETERS:
            messages: List of messages (conversation history)
            max_tokens: Maximum tokens in response

        RETURNS: LLMResponse or None on error
        """
        pass

    @abstractmethod
    def get_model_name(self) -> str:
        """
        Get model identifier

        WHAT: Return model name/version
        WHY:  For logging and debugging
        HOW:  Provider-specific identifier

        RETURNS: Model name string
        """
        pass


class AnthropicProvider(LLMProvider):
    """
    Anthropic Claude LLM provider

    WHAT: Claude API implementation
    WHY:  Default provider for Code Surgeon
    HOW:  Wraps Anthropic SDK
    """

    def __init__(self, api_key: Optional[str] = None, model: str = "claude-3-5-haiku-20241022"):
        """
        Initialize Anthropic provider

        WHAT: Setup Claude API client
        WHY:  Configure authentication and model
        HOW:  Load API key from env or parameter

        PARAMETERS:
            api_key: Anthropic API key (defaults to ANTHROPIC_API_KEY env var)
            model: Claude model to use
        """
        self.api_key = api_key or os.environ.get("ANTHROPIC_API_KEY")
        self.model = model
        self._client = None

    def _get_client(self):
        """
        Lazy load Anthropic client

        WHAT: Create client on first use
        WHY:  Avoid import/initialization overhead
        HOW:  Check and create if needed
        """
        if self._client is None:
            import anthropic
            if not self.api_key:
                raise ValueError("ANTHROPIC_API_KEY not set")
            self._client = anthropic.Anthropic(api_key=self.api_key)
        return self._client

    def generate(self, messages: List[LLMMessage], max_tokens: int = 4096) -> Optional[LLMResponse]:
        """
        Generate response using Claude

        WHAT: Call Claude API with messages
        WHY:  Implement LLMProvider interface
        HOW:  Convert messages to Anthropic format and call API

        PARAMETERS:
            messages: Conversation messages
            max_tokens: Response length limit

        RETURNS: LLMResponse or None on error
        SIDE EFFECTS: API call to Anthropic
        """
        try:
            client = self._get_client()

            # Convert to Anthropic format
            anthropic_messages = [
                {"role": msg.role, "content": msg.content}
                for msg in messages
            ]

            response = client.messages.create(
                model=self.model,
                max_tokens=max_tokens,
                messages=anthropic_messages
            )

            return LLMResponse(
                text=response.content[0].text,
                model=self.model,
                tokens_used=response.usage.total_tokens if hasattr(response, 'usage') else None,
                finish_reason=response.stop_reason if hasattr(response, 'stop_reason') else None,
                raw_response={"id": response.id, "type": response.type} if hasattr(response, 'id') else None
            )

        except Exception as e:
            print(f"❌ Anthropic API error: {e}")
            return None

    def get_model_name(self) -> str:
        """Return Claude model name"""
        return self.model


class MockLLMProvider(LLMProvider):
    """
    Mock LLM for testing

    WHAT: Fake LLM that returns predefined responses
    WHY:  Enable testing without API calls
    HOW:  Return canned responses
    """

    def __init__(self, fixed_response: str = "Mock LLM response"):
        """
        Initialize mock provider

        WHAT: Setup mock with fixed response
        WHY:  Deterministic testing
        HOW:  Store response string
        """
        self.fixed_response = fixed_response

    def generate(self, messages: List[LLMMessage], max_tokens: int = 4096) -> Optional[LLMResponse]:
        """
        Return mock response

        WHAT: Return predefined text
        WHY:  Testing without API calls
        HOW:  Wrap fixed_response in LLMResponse
        """
        return LLMResponse(
            text=self.fixed_response,
            model="mock-model",
            tokens_used=len(self.fixed_response.split()),
            finish_reason="mock_complete",
            raw_response=None
        )

    def get_model_name(self) -> str:
        """Return mock model name"""
        return "mock-llm-v1.0"


class OpenAIProvider(LLMProvider):
    """
    OpenAI GPT provider

    WHAT: GPT-5 API implementation
    WHY:  Use OpenAI's latest model for Code Surgeon
    HOW:  Wraps OpenAI SDK
    """

    def __init__(self, api_key: Optional[str] = None, model: str = "gpt-5"):
        """
        Initialize OpenAI provider

        WHAT: Setup OpenAI client
        WHY:  Connect to OpenAI API
        HOW:  Use api_key or OPENAI_API_KEY env var

        PARAMETERS:
            api_key: Optional API key (defaults to env var)
            model: Model name (default: gpt-5)
        """
        self.api_key = api_key or os.environ.get("OPENAI_API_KEY")
        self.model = model

        if not self.api_key:
            raise ValueError("OpenAI API key required (set OPENAI_API_KEY environment variable)")

    def generate(self, messages: List[LLMMessage], max_tokens: int = 4096) -> Optional[LLMResponse]:
        """
        Generate response from GPT-5

        WHAT: Call OpenAI API
        WHY:  Get LLM completion
        HOW:  Use openai.chat.completions.create

        PARAMETERS:
            messages: Conversation history
            max_tokens: Max response tokens

        RETURNS: LLMResponse or None on error
        """
        try:
            import openai
            client = openai.OpenAI(api_key=self.api_key)

            # Convert to OpenAI format
            openai_messages = [
                {"role": msg.role, "content": msg.content}
                for msg in messages
            ]

            response = client.chat.completions.create(
                model=self.model,
                messages=openai_messages,
                max_completion_tokens=max_tokens
            )

            return LLMResponse(
                text=response.choices[0].message.content,
                model=self.model,
                tokens_used=response.usage.total_tokens if hasattr(response, 'usage') else None,
                finish_reason=response.choices[0].finish_reason if response.choices else None,
                raw_response={"id": response.id} if hasattr(response, 'id') else None
            )

        except Exception as e:
            print(f"❌ OpenAI API error: {e}")
            return None

    def get_model_name(self) -> str:
        """Return GPT model name"""
        return self.model


def create_llm_provider(provider_type: str = "openai", **kwargs) -> LLMProvider:
    """
    Factory function for LLM providers

    WHAT: Create LLM provider by name
    WHY:  Simple provider instantiation
    HOW:  Match provider_type to class

    PARAMETERS:
        provider_type: "anthropic" | "openai" | "mock"
        **kwargs: Provider-specific arguments

    RETURNS: LLMProvider instance

    EXAMPLE:
        provider = create_llm_provider("anthropic", model="claude-3-opus-20240229")
        provider = create_llm_provider("mock", fixed_response="test response")
    """
    providers = {
        "anthropic": AnthropicProvider,
        "openai": OpenAIProvider,
        "mock": MockLLMProvider,
    }

    provider_class = providers.get(provider_type.lower())
    if not provider_class:
        raise ValueError(f"Unknown provider type: {provider_type}")

    return provider_class(**kwargs)
