using DnDLibrary.Engine;
using DnDLibrary.System;
using Microsoft.Extensions.Logging;
using Xunit;

namespace DnD.Test.Tests.Engine
{
    public class DiceTest
    {
        [Theory]
        [InlineData(1)]
        [InlineData(2)]
        [InlineData(3)]
        [InlineData(4)]
        [InlineData(5)]
        [InlineData(6)]
        public void rollD4(int rolls)
        {
            ILoggerFactory loggerloggerFactory = new Microsoft.Extensions.Logging.Abstractions.NullLoggerFactory();
            IRng rng = new Rng();
            Dice testdice = new Dice(rng, loggerloggerFactory);
            var result = testdice.D4(rolls);

            Assert.Multiple(
                () => Assert.Equal(result.Count(), rolls),
                () => Assert.True(result.Sum() <= (rolls * 4)),
                () => Assert.True(result.Sum() > 0)
                );
        }
        [Theory]
        [InlineData(1)]
        [InlineData(2)]
        [InlineData(3)]
        [InlineData(4)]
        [InlineData(5)]
        [InlineData(6)]
        public void rollD6(int rolls)
        {
            ILoggerFactory loggerloggerFactory = new Microsoft.Extensions.Logging.Abstractions.NullLoggerFactory();
            IRng rng = new Rng();
            Dice testdice = new Dice(rng, loggerloggerFactory);
            var result = testdice.D6(rolls);

            Assert.Multiple(
                () => Assert.Equal(result.Count(), rolls),
                () => Assert.True(result.Sum() <= (rolls * 6)),
                () => Assert.True(result.Sum() > 0)
                );
        }
        [Theory]
        [InlineData(1)]
        [InlineData(2)]
        [InlineData(3)]
        [InlineData(4)]
        [InlineData(5)]
        [InlineData(6)]
        public void rollD8(int rolls)
        {
            ILoggerFactory loggerloggerFactory = new Microsoft.Extensions.Logging.Abstractions.NullLoggerFactory();
            IRng rng = new Rng();
            Dice testdice = new Dice(rng, loggerloggerFactory);
            var result = testdice.D8(rolls);

            Assert.Multiple(
                () => Assert.Equal(result.Count(), rolls),
                () => Assert.True(result.Sum() <= (rolls * 8)),
                () => Assert.True(result.Sum() > 0)
                );
        }
        [Theory]
        [InlineData(1)]
        [InlineData(2)]
        [InlineData(3)]
        [InlineData(4)]
        [InlineData(5)]
        [InlineData(6)]
        public void rollD12(int rolls)
        {
            ILoggerFactory loggerloggerFactory = new Microsoft.Extensions.Logging.Abstractions.NullLoggerFactory();
            IRng rng = new Rng();
            Dice testdice = new Dice(rng, loggerloggerFactory);
            var result = testdice.D12(rolls);

            Assert.Multiple(
                () => Assert.Equal(result.Count(), rolls),
                () => Assert.True(result.Sum() <= (rolls * 12)),
                () => Assert.True(result.Sum() > 0)
                );
        }
        [Theory]
        [InlineData(1)]
        [InlineData(2)]
        [InlineData(3)]
        [InlineData(4)]
        [InlineData(5)]
        [InlineData(6)]
        public void rollD100(int rolls)
        {
            ILoggerFactory loggerloggerFactory = new Microsoft.Extensions.Logging.Abstractions.NullLoggerFactory();
            IRng rng = new Rng();
            Dice testdice = new Dice(rng, loggerloggerFactory);
            var result = testdice.D100(rolls);
            var total = testdice.RollTotal(result);

            Assert.Multiple(
                () => Assert.Equal(result.Count(), rolls),
                () => Assert.True(total <= (rolls * 100)),
                () => Assert.True(total > 0)
                );
        }
        [Theory]
        [InlineData(1)]
        [InlineData(2)]
        [InlineData(3)]
        [InlineData(4)]
        [InlineData(5)]
        [InlineData(6)]
        public void rollD20(int rolls)
        {
            ILoggerFactory loggerloggerFactory = new Microsoft.Extensions.Logging.Abstractions.NullLoggerFactory();
            IRng rng = new Rng();
            Dice testdice = new Dice(rng, loggerloggerFactory);
            var result = testdice.D20(rolls);
            var total = testdice.RollTotal(result);

            Assert.Multiple(
                () => Assert.Equal(result.Count(), rolls),
                () => Assert.True(total <= (rolls * 20)),
                () => Assert.True(total > 0)
                );
        }
    }
}
